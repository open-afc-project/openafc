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
import os
import uvicorn
import sys
from typing import Optional

from rcache_common import dp, get_module_logger, set_dp_printer
from rcache_models import IfDbExists, RcacheServiceSettings, RcacheUpdateReq, \
    RcacheInvalidateReq, RcacheSpatialInvalidateReq
from rcache_service import ReqCacheService

__all__ = ["app"]

LOGGER = get_module_logger()

# Parameters (passed via environment variables)
settings = RcacheServiceSettings()

# Request cache object (created upon first request)
rcache_service: Optional[ReqCacheService] = None


def get_service() -> ReqCacheService:
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
            ReqCacheService(
                rcache_db_dsn=settings.postgres_dsn,
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
    set_dp_printer(lambda s: fastapi.logger.logger.error(s))
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
def shutdown() -> None:
    """ App shutdown event handler """
    if not settings.enabled:
        return
    get_service().disconnect_db()


@app.get("/healthcheck")
async def healthcheck() -> None:
    """ Invalidate request handler """
    pass


@app.post("/invalidate")
async def invalidate(
        invalidate_req: RcacheInvalidateReq,
        service: ReqCacheService = fastapi.Depends(get_service)) -> None:
    """ Invalidate request handler """
    service.invalidate(invalidate_req)


@app.post("/spatial_invalidate")
async def spatial_invalidate(
        spatial_invalidate_req: RcacheSpatialInvalidateReq,
        service: ReqCacheService = fastapi.Depends(get_service)) -> None:
    """ Spatial invalidate request handler """
    service.invalidate(spatial_invalidate_req)


@app.post("/update")
async def update(
        update_req: RcacheUpdateReq,
        service: ReqCacheService = fastapi.Depends(get_service)) -> None:
    """ Cache update request handler """
    service.update(update_req)


if __name__ == "__main__":
    # Autonomous startup
    uvicorn.run(app, host="0.0.0.0", port=settings.port)
