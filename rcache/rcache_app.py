#!/usr/bin/env python3
""" AFC Request cache """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=wrong-import-order, global-statement, unnecessary-pass

import fastapi
import logging
import uvicorn
import sys
from typing import Annotated, Optional

from log_utils import dp, get_module_logger, set_dp_printer, set_parent_logger
from rcache_models import IfDbExists, RcacheServiceSettings, RcacheUpdateReq, \
    RcacheInvalidateReq, RcacheSpatialInvalidateReq, RcacheStatus
from rcache_service import RcacheService

__all__ = ["app"]

PARENT_LOGGER = "uvicorn.error"
set_parent_logger(PARENT_LOGGER)
LOGGER = get_module_logger()

# Parameters (passed via environment variables)
settings = RcacheServiceSettings()

# Request cache object (created upon first request)
rcache_service: Optional[RcacheService] = None


def get_service() -> RcacheService:
    """ Returns service object (creating it on first call """
    global rcache_service
    if not settings.enabled:
        raise \
            fastapi.HTTPException(
                fastapi.status.HTTP_503_SERVICE_UNAVAILABLE,
                detail="Rcache not enabled (RCACHE_ENABLED environment "
                "variable set to False")
    if rcache_service is None:
        rcache_service = \
            RcacheService(
                rcache_db_dsn=settings.postgres_dsn,
                rcache_db_password_file=settings.postgres_password_file,
                precompute_quota=settings.precompute_quota,
                afc_req_url=settings.afc_req_url,
                rulesets_url=settings.rulesets_url,
                config_retrieval_url=settings.config_retrieval_url)
    return rcache_service


# FastAPI APP
app = fastapi.FastAPI()


@app.on_event("startup")
async def startup() -> None:
    """ App startup event handler """
    set_dp_printer(fastapi.logger.logger.error)
    logging.getLogger(PARENT_LOGGER).setLevel("INFO")
    if not settings.enabled:
        return
    service = get_service()
    if not service.check_db_server():
        LOGGER.error("Can't connect to postgres database server")
        sys.exit()
    await service.connect_db(
        create_if_absent=True,
        recreate_db=settings.if_db_exists == IfDbExists.recreate,
        recreate_tables=settings.if_db_exists == IfDbExists.clean)


@app.on_event("shutdown")
async def shutdown() -> None:
    """ App shutdown event handler """
    if not settings.enabled:
        return
    await get_service().shutdown()


@app.get("/healthcheck")
async def healthcheck(
        response: fastapi.Response,
        service: RcacheService = fastapi.Depends(get_service)) -> None:
    """ Healthcheck """
    response.status_code = fastapi.status.HTTP_200_OK if service.healthy() \
        else fastapi.status.HTTP_500_INTERNAL_SERVER_ERROR


@app.post("/invalidate")
async def invalidate(
        invalidate_req: RcacheInvalidateReq,
        service: RcacheService = fastapi.Depends(get_service)) -> None:
    """ Invalidate request handler """
    service.invalidate(invalidate_req)


@app.post("/spatial_invalidate")
async def spatial_invalidate(
        spatial_invalidate_req: RcacheSpatialInvalidateReq,
        service: RcacheService = fastapi.Depends(get_service)) -> None:
    """ Spatial invalidate request handler """
    service.invalidate(spatial_invalidate_req)


@app.post("/update")
async def update(
        update_req: RcacheUpdateReq,
        service: RcacheService = fastapi.Depends(get_service)) -> None:
    """ Cache update request handler """
    service.update(update_req)


@app.post("/invalidation_state/{enabled}")
async def set_invalidation_state(
        enabled: Annotated[
            bool,
            fastapi.Path(
                title="true/false to enable/disable invalidation")],
        service: RcacheService = fastapi.Depends(get_service)) -> None:
    """ Enable/disable invalidation """
    await service.set_invalidation_enabled(enabled)


@app.get("/invalidation_state")
async def get_invalidation_state(
        service: RcacheService = fastapi.Depends(get_service)) -> bool:
    """ Return invalidation enabled state """
    return await service.get_invalidation_enabled()


@app.post("/precomputation_state/{enabled}")
async def set_precomputation_state(
        enabled: Annotated[
            bool,
            fastapi.Path(
                title="true/false to enable/disable precomputation")],
        service: RcacheService = fastapi.Depends(get_service)) -> None:
    """ Enable/disable precomputation """
    await service.set_precomputation_enabled(enabled)


@app.get("/precomputation_state")
async def get_precomputation_state(
        service: RcacheService = fastapi.Depends(get_service)) -> bool:
    """ Return precomputation enabled state """
    return await service.get_precomputation_enabled()


@app.post("/update_state/{enabled}")
async def set_update_state(
        enabled: Annotated[
            bool,
            fastapi.Path(
                title="true/false to enable/disable update")],
        service: RcacheService = fastapi.Depends(get_service)) -> None:
    """ Enable/disable update """
    await service.set_update_enabled(enabled)


@app.get("/update_state")
async def get_update_enabled_state(
        service: RcacheService = fastapi.Depends(get_service)) -> bool:
    """ Return update enabled state """
    return await service.get_update_enabled()


@app.get("/status")
async def get_service_status(
        service: RcacheService = fastapi.Depends(get_service)) \
        -> RcacheStatus:
    """ Get Rcache service status information """
    return await service.get_status()


@app.get("/precomputation_quota")
async def get_precompute_quota(
            service: RcacheService = fastapi.Depends(get_service)) -> int:
    """ Returns current precomputation quota (maximum number of simultaneously
    running precomputations) """
    return service.precompute_quota


@app.post("/precomputation_quota/{quota}")
async def set_precompute_quota(
        quota: Annotated[
            int,
            fastapi.Path(
                title="Maximum number of simultaneous precomputations",
                ge=0)],
        service: RcacheService = fastapi.Depends(get_service)) -> None:
    """ Sets new precomputation quota (maximum number of simultaneously running
    precomputations) """
    service.precompute_quota = quota


if __name__ == "__main__":
    # Autonomous startup
    uvicorn.run(app, host="0.0.0.0", port=settings.port, log_level="info")
