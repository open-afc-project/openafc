""" Sending requests to ReqCacher service """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

import pydantic
import requests
from typing import Any, Dict, List, Optional

from rcache_common import dp, error, FailOnError
from rcache_models import AfcReqRespKey, RcacheUpdateReq, \
    RcacheInvalidateReq, LatLonRect, RcacheSpatialInvalidateReq


class ReqCacheRcache:
    """ Communicates with Request cache service

    Private attributes:
    _rcache_server_url -- Request cache service URL
    """
    def __init__(self, rcache_server_url: str) -> None:
        self._rcache_server_url = rcache_server_url.rstrip("/")

    def update_cache(self, rrks: List[AfcReqRespKey],
                     fail_on_error: bool = True) -> bool:
        """ Update Request cache

        Arguments:
        rrks          -- List of request/response/digest triplets
        fail_on_error -- True to fail on error, False to return False
        Returns True on success, False on known fail if fail_on_error is False
        """
        with FailOnError(fail_on_error):
            try:
                self._post(command="update",
                           json=RcacheUpdateReq(req_resp_keys=rrks).dict())
            except pydantic.ValidationError as ex:
                error(f"Invalid argument format: {ex}")
            return True
        return False

    def invalidate_cache(self, ruleset_ids: Optional[List[str]] = None,
                         fail_on_error: bool = True) -> bool:
        """ Invalidate request cache (completely of for config)

        Arguments:
        ruleset_ids   -- None for complete invalidation, list of ruleset IDs
                         for configs to invalidate for config-based
                         invalidation
        fail_on_error -- True to fail on error, False to return False
        Returns True on success, False on known fail if fail_on_error is False
        """
        with FailOnError(fail_on_error):
            try:
                self._post(
                    command="invalidate",
                    json=RcacheInvalidateReq(ruleset_ids=ruleset_ids).dict())
            except pydantic.ValidationError as ex:
                error(f"Invalid argument format: {ex}")
            return True
        return False

    def spatial_invalidate_cache(self, tiles: List[LatLonRect],
                                 fail_on_error: bool = True) -> bool:
        """ Spatial invalidation of request cache

        Arguments:
        tiles         -- List of tiles, containing changed FSs
        fail_on_error -- True to fail on error, False to return False
        Returns True on success, False on known fail if fail_on_error is False
        """
        with FailOnError(fail_on_error):
            try:
                self._post(command="spatial_invalidate",
                           json=RcacheSpatialInvalidateReq(tiles=tiles).dict())
            except pydantic.ValidationError as ex:
                error(f"Invalid argument format: {ex}")
            return True
        return False

    def _post(self, command: str, json: Dict[str, Any]) -> None:
        """ Do the POST request to Request cache service

        Arguments:
        command -- Command (last part of URL) to invoke
        json    -- Command parameters in JSON format
        """
        try:
            requests.post(f"{self._rcache_server_url}/{command}", json=json)
        except requests.RequestException as ex:
            error(f"Error sending '{command}' post to Request cache Server: "
                  f"{ex}")
