#!/usr/bin/env python3
""" AFC Request server """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=wrong-import-order, global-statement, too-many-arguments

import fastapi
import logging
import uvicorn
from typing import Any, Dict, Optional


import afc_server_compute
import afc_server_db
import afc_server_models
import afc_server_msg_proc
import appcfg
from log_utils import dp, get_module_logger, set_dp_printer, set_parent_logger


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
                ratdb_dsn=settings.ratdb_dsn,
                ratdb_password_file=settings.ratdb_password_file,
                rcache_dsn=settings.rcache_dsn,
                rcache_password_file=settings.rcache_password_file,
                bypass_cert=settings.bypass_cert,
                bypass_rcache=settings.bypass_rcache)
        compute = \
            afc_server_compute.AfcServerCompute(
                rmq_dsn=settings.rmq_dsn,
                rmq_password_file=settings.rmq_password_file,
                engine_request_type=settings.engine_request_type,
                worker_mnt_root=settings.static_data_root or
                appcfg.NFS_MOUNT_PATH)
        g_message_processor = \
            afc_server_msg_proc.AfcServerMessageProcessor(
                 db=db, compute=compute,
                 request_timeout_sec=settings.request_timeout,
                 edebug_request_timeout_sec=settings.request_timeout_edebug,
                 config_refresh_sec=settings.config_refresh)
    return g_message_processor


# FastAPI APP
app = fastapi.FastAPI()


@app.on_event("startup")
async def startup() -> None:
    """ App startup event handler """
    set_dp_printer(fastapi.logger.logger.error)
    if settings.log_level is not None:
        logging.getLogger(PARENT_LOGGER).setLevel(settings.log_level.upper())


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
    response.status_code = fastapi.status.HTTP_200_OK


@app.post("/fbrat/ap-afc/availableSpectrumInquiry",
          summary="Process AFC Request from outside the cluster "
          "(msghnd-compatible path)")
@app.post("/availableSpectrumInquiry",
          summary="Process AFC Request from outside the cluster")
async def available_pectrum_inquiry(
        afc_req_msg: afc_server_models.Rest_ReqMsg,
        debug: bool = fastapi.Query(
            False, title="Run request in AFC Engine in debug mode"),
        edebug: bool = fastapi.Query(
            False, title="Run request in AFC Engine in extended debug mode"),
        nocache: bool = fastapi.Query(
            False, title="Run request in AFC Engine (bypass cache lookup)"),
        gui: bool = fastapi.Query(
            False, title="Request from Web GUI"),
        message_processor: afc_server_msg_proc.AfcServerMessageProcessor =
        fastapi.Depends(get_message_processor)) -> Dict[str, Any]:
    """ Process external AFC Request message """
    return \
        await message_processor.process_msg(
            req_msg=afc_req_msg, debug=debug, edebug=edebug,
            nocache=nocache, gui=gui, internal=False)


@app.post("/fbrat/ap-afc/availableSpectrumInquiryInternal",
          summary="Process AFC Request from inside the cluster (legacy path)")
@app.post("/availableSpectrumInquiryInternal",
          summary="Process AFC Request from inside the cluster")
async def available_pectrum_inquiry_internal(
        afc_req_msg: afc_server_models.Rest_ReqMsg,
        debug: bool = fastapi.Query(
            False, title="Run request in AFC Engine in debug mode"),
        edebug: bool = fastapi.Query(
            False, title="Run request in AFC Engine in extended debug mode"),
        nocache: bool = fastapi.Query(
            False, title="Run request in AFC Engine (bypass cache lookup)"),
        gui: bool = fastapi.Query(
            False, title="Request from Web GUI"),
        message_processor: afc_server_msg_proc.AfcServerMessageProcessor =
        fastapi.Depends(get_message_processor)) -> Dict[str, Any]:
    """ Process internal AFC Request message """
    return \
        await message_processor.process_msg(
            req_msg=afc_req_msg, debug=debug, edebug=edebug,
            nocache=nocache, gui=gui, internal=True)


if __name__ == "__main__":
    # Autonomous startup
    uvicorn.run(app, host="0.0.0.0", port=settings.port, log_level="info")
