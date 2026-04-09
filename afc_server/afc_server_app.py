#!/usr/bin/env python3
""" AFC Request server """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=wrong-import-order, global-statement, too-many-arguments
# pylint: disable=too-many-positional-arguments

import prometheus_utils
from log_utils import dp, get_module_logger, set_dp_printer, set_parent_logger
import appcfg
import afc_traffic_metrics
import afc_server_msg_proc
from afcmodels import afc_server_models
import afc_server_db
import afc_server_compute
import os
import hmac
import fastapi
import logging
import time
import uvicorn
from typing import Any, Awaitable, Callable, Dict, Optional, Union


def _get_internal_token() -> Optional[str]:
    """Read AFC_INTERNAL_TOKEN from file (AFC_INTERNAL_TOKEN_FILE) or env."""
    file_path = os.environ.get("AFC_INTERNAL_TOKEN_FILE")
    if file_path:
        try:
            with open(file_path) as fh:
                return fh.read().strip() or None
        except OSError:
            pass
    return os.environ.get("AFC_INTERNAL_TOKEN") or None


def _get_dispatcher_token() -> Optional[str]:
    """Read AFC_DISPATCHER_TOKEN from file (AFC_DISPATCHER_TOKEN_FILE) or env.

    This token is mounted ONLY into the nginx dispatcher and afc_server/msghnd
    (not shared with other AFC_INTERNAL_TOKEN holders), so its presence on a
    request proves that mTLS-DN / X-SSL-Client-Verify were set by nginx.
    """
    file_path = os.environ.get("AFC_DISPATCHER_TOKEN_FILE")
    if file_path:
        try:
            with open(file_path) as fh:
                return fh.read().strip() or None
        except OSError:
            pass
    return os.environ.get("AFC_DISPATCHER_TOKEN") or None


__all__ = ["app"]

settings = afc_server_models.AfcServerSettings()
g_message_processor: \
    Optional[afc_server_msg_proc.AfcServerMessageProcessor] = None

PARENT_LOGGER = "uvicorn.error"
set_parent_logger(PARENT_LOGGER)
LOGGER = get_module_logger()


async def get_message_processor() \
        -> afc_server_msg_proc.AfcServerMessageProcessor:
    """ Returns message processor object """
    global g_message_processor
    if g_message_processor is None:
        db = \
            afc_server_db.AfcServerDb(
                ratdb_dsn=str(settings.ratdb_dsn),
                ratdb_password_file=settings.ratdb_password_file,
                rcache_dsn=str(settings.rcache_dsn),
                rcache_password_file=settings.rcache_password_file,
                bypass_cert=settings.bypass_cert,
                bypass_rcache=settings.bypass_rcache,
                return_invalidated=bool(
                    settings.afc_state_vendor_extensions))
        compute = \
            afc_server_compute.AfcServerCompute(
                rmq_dsn=str(settings.rmq_dsn),
                rmq_password_file=settings.rmq_password_file,
                engine_request_type=settings.engine_request_type,
                worker_mnt_root=settings.static_data_root or
                appcfg.NFS_MOUNT_PATH)
        g_message_processor = \
            afc_server_msg_proc.AfcServerMessageProcessor(
                db=db, compute=compute,
                request_timeout_sec=settings.request_timeout,
                edebug_request_timeout_sec=settings.request_timeout_edebug,
                config_refresh_sec=settings.config_refresh,
                afc_state_vendor_extensions=settings.
                afc_state_vendor_extensions)
    return g_message_processor


# FastAPI APP
app = fastapi.FastAPI()


@app.on_event("startup")
async def startup() -> None:
    """ App startup event handler """
    set_dp_printer(fastapi.logger.logger.error)
    appcfg.install_credential_redact_filter()
    if settings.log_level is not None:
        logging.getLogger(PARENT_LOGGER).setLevel(settings.log_level.upper())
    _token = _get_internal_token()
    if not _token:
        logging.getLogger(PARENT_LOGGER).critical(
            "FATAL: AFC_INTERNAL_TOKEN is not set. The internal endpoint "
            "cannot be secured without it. Set AFC_INTERNAL_TOKEN_FILE to "
            "a Docker secret path, or set AFC_INTERNAL_TOKEN. "
            "Generate a random secret:  python3 -c \"import secrets; "
            "print(secrets.token_hex(32))\"")
        raise SystemExit(1)
    if os.environ.get("AFC_ENABLE_TEST_CERTS", "").lower() in ("1", "true", "yes"):
        logging.getLogger(PARENT_LOGGER).critical(
            "SECURITY WARNING: AFC_ENABLE_TEST_CERTS is enabled. "
            "TestCertificationId / TestSerialNumber are accepted without "
            "registration. This setting is intended only for development/CI; "
            "must never be active in production deployments.")


@app.on_event("shutdown")
async def shutdown() -> None:
    """ App shutdown event handler """
    global g_message_processor
    if g_message_processor is not None:
        await g_message_processor.close()
    g_message_processor = None


@app.get("/fbrat/ap-afc/healthy",
         summary="200 if somehow alive (msghnd-compatible path)")
@app.get("/healthy", summary="200 if somehow alive")
async def healthcheck(response: fastapi.Response) -> None:
    """ Healthcheck """
    # Surface a dead RMQ reader so orchestrators (Docker compose,
    # k8s) recycle the worker.  A reader task that has exited while not in
    # the shutdown path means all future compute requests will time out with
    # GENERAL_FAILURE.
    mp = g_message_processor
    if mp is not None and \
            mp._compute._rmq_reader_task.done() and \
            not mp._compute._stopping:
        response.status_code = fastapi.status.HTTP_503_SERVICE_UNAVAILABLE
        return
    response.status_code = fastapi.status.HTTP_200_OK


@app.post("/fbrat/ap-afc/availableSpectrumInquiry",
          summary="Process AFC Request from outside the cluster "
          "(msghnd-compatible path)")
@app.post("/availableSpectrumInquiry",
          summary="Process AFC Request from outside the cluster")
async def available_spectrum_inquiry(
        afc_req_msg: afc_server_models.Rest_ReqMsg,
        mtls_dn: Optional[str] = fastapi.Header(default=None),
        x_real_ip: Optional[str] = fastapi.Header(default=None),
        x_ssl_client_verify: Optional[str] = fastapi.Header(default=None),
        x_afc_precompute: Optional[str] = fastapi.Header(default=None),
        x_afc_internal_token: Optional[str] = fastapi.Header(default=None),
        x_afc_dispatcher_token: Optional[str] = fastapi.Header(default=None),
        message_processor: afc_server_msg_proc.AfcServerMessageProcessor =
        fastapi.Depends(get_message_processor)) -> Dict[str, Any]:
    """ Process external AFC Request message """
    # Require the gateway token that nginx sets via proxy_set_header.
    # Requests not routed through nginx will not carry this token.
    expected_token = _get_internal_token()
    if not expected_token:
        raise fastapi.HTTPException(
            status_code=503,
            detail="Gateway token unavailable")
    supplied = x_afc_internal_token or ""
    if not hmac.compare_digest(supplied, expected_token):
        raise fastapi.HTTPException(
            status_code=403,
            detail="Missing or invalid gateway token")
    # mTLS-DN / X-SSL-Client-Verify below are nginx attestation headers; only
    # honour them when the request also carries the nginx-only
    # AFC_DISPATCHER_TOKEN. AFC_INTERNAL_TOKEN alone is insufficient because it
    # is shared with non-gateway cluster services.
    expected_disp = _get_dispatcher_token()
    if not expected_disp or not hmac.compare_digest(
            x_afc_dispatcher_token or "", expected_disp):
        raise fastapi.HTTPException(
            status_code=403,
            detail="Missing or invalid dispatcher token")
    # When AFC_ENFORCE_MTLS is true nginx requires a valid client cert; the
    # afc_server should reject requests that lack the forwarded mTLS-DN header
    # as an additional server-side enforcement layer.
    if settings.enforce_mtls and not mtls_dn:
        raise fastapi.HTTPException(
            status_code=403,
            detail="mTLS client certificate required")
    # Trust mTLS-DN only when nginx attests successful verification.
    # Reject any request that supplies an mTLS-DN without a verify header that
    # explicitly says SUCCESS. A previous version only rejected when the verify
    # header was *present* and != SUCCESS, so an absent header now also triggers
    # rejection when an mTLS-DN is supplied.
    if mtls_dn and x_ssl_client_verify != "SUCCESS":
        raise fastapi.HTTPException(
            status_code=403,
            detail="mTLS certificate verification failed")
    return \
        await message_processor.process_msg(
            req_msg=afc_req_msg, debug=False, edebug=False,
            gui=False, mtls_dn=mtls_dn, ap_ip=x_real_ip,
            internal=False,
            low_priority=x_afc_precompute is not None)


@app.post("/fbrat/ap-afc/availableSpectrumInquiryInternal",
          summary="Process AFC Request from inside the cluster (legacy path)")
@app.post("/availableSpectrumInquiryInternal",
          summary="Process AFC Request from inside the cluster")
async def available_spectrum_inquiry_internal(
        afc_req_msg: afc_server_models.Rest_ReqMsg,
        debug: bool = fastapi.Query(
            False, title="Run request in AFC Engine in debug mode"),
        edebug: bool = fastapi.Query(
            False, title="Run request in AFC Engine in extended debug mode"),
        gui: bool = fastapi.Query(
            False, title="Request from Web GUI"),
        mtls_dn: Optional[str] = fastapi.Header(default=None),
        x_real_ip: Optional[str] = fastapi.Header(default=None),
        x_afc_internal_token: Optional[str] = fastapi.Header(default=None),
        x_afc_dispatcher_token: Optional[str] = fastapi.Header(default=None),
        message_processor: afc_server_msg_proc.AfcServerMessageProcessor =
        fastapi.Depends(get_message_processor)) -> Dict[str, Any]:
    """ Process internal AFC Request message """
    expected_token = _get_internal_token()
    supplied = x_afc_internal_token or ""
    if not expected_token or not hmac.compare_digest(supplied, expected_token):
        raise fastapi.HTTPException(status_code=403, detail="Forbidden")
    # AFC_INTERNAL_TOKEN is shared with non-gateway cluster services; only
    # grant is_internal privileges (openAfc.overrideAfcConfig, debug/edebug,
    # cert-bypass) when the caller ALSO presents the narrower dispatcher
    # token. Otherwise fall back to external-caller semantics.
    expected_disp = _get_dispatcher_token()
    is_internal = bool(expected_disp) and hmac.compare_digest(
        x_afc_dispatcher_token or "", expected_disp)
    if not is_internal:
        debug = edebug = gui = False
    return \
        await message_processor.process_msg(
            req_msg=afc_req_msg, debug=debug, edebug=edebug,
            gui=gui, mtls_dn=mtls_dn, ap_ip=x_real_ip,
            internal=is_internal)


# Exposing Prometheus metrics behind X-AFC-Internal-Token gate so only
# authorised scrapers (Prometheus, configured with bearer_token_file) can read.
def _make_token_gated_metrics_app(
        metrics_app: Any) -> Any:
    """Wrap a metrics ASGI app with X-AFC-Internal-Token verification."""
    async def _gated(scope: Any, receive: Any, send: Any) -> None:
        if scope.get("type") == "http":
            expected = _get_internal_token()
            headers = dict(scope.get("headers", []))
            auth_header = headers.get(b"x-afc-internal-token", b"").decode()
            if not expected or not hmac.compare_digest(auth_header, expected):
                response = fastapi.Response(status_code=403,
                                            content="Forbidden")
                await response(scope, receive, send)
                return
        await metrics_app(scope, receive, send)
    return _gated


app.mount("/metrics",
          _make_token_gated_metrics_app(
              prometheus_utils.multiprocess_fastapi_metrics()))


@app.middleware("http")
async def add_process_time_header(
        request: fastapi.Request,
        call_next: Callable[[fastapi.Request], Awaitable[fastapi.Response]]) \
        -> fastapi.Response:
    """ Middleware that updates message-level AFC traffic metrics """
    is_afc = \
        any(request.url.path.endswith(afc_path) for afc_path in
            ("availableSpectrumInquiry", "availableSpectrumInquiryInternal"))
    start_time = time.time()
    status: Union[int, str] = "Exception"
    try:
        response = await call_next(request)
        status = response.status_code
        return response
    finally:
        if is_afc:
            afc_traffic_metrics.message_processed(
                duration_sec=time.time() - start_time, status=status)


if __name__ == "__main__":
    # Autonomous startup
    uvicorn.run(app, host="0.0.0.0", port=settings.port, log_level="info")
