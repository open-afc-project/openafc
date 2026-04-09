""" Sending requests to Rcache service """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=wrong-import-order

import pydantic
import requests
from typing import Any, Dict, List, Optional

from log_utils import error, FailOnError
from rcache_models import AfcReqRespKey, Beam, LatLonRect, \
    RcacheDirectionalInvalidateReq, RcacheInvalidateReq, \
    RcacheSpatialInvalidateReq, RcacheUpdateReq


class RcacheRcache:
    """ Communicates with Request cache service

    Private attributes:
    _rcache_server_url -- Request cache service URL
    _api_key           -- API key for authentication
    """

    def __init__(self, rcache_server_url: str, api_key_file: Optional[str] = None) -> None:
        self._rcache_server_url = rcache_server_url.rstrip("/")
        self._api_key = None
        if api_key_file:
            try:
                with open(api_key_file, "r") as f:
                    self._api_key = f.read().strip()
            except Exception as ex:
                error(f"Failed to read API key from {api_key_file}: {ex}")

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
                           json=RcacheUpdateReq(req_resp_keys=rrks).model_dump())
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
                    json=RcacheInvalidateReq(ruleset_ids=ruleset_ids).model_dump())
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
                           json=RcacheSpatialInvalidateReq(tiles=tiles).model_dump())
            except pydantic.ValidationError as ex:
                error(f"Invalid argument format: {ex}")
            return True
        return False

    def directional_invalidate_cache(self, beams: List[Beam],
                                     fail_on_error: bool = True) -> bool:
        """ Directional invalidation of request cache

        Arguments:
        beams         -- List of RX beams of changed FSs
        fail_on_error -- True to fail on error, False to return False
        Returns True on success, False on known fail if fail_on_error is False
        """
        # RcacheDirectionalInvalidateReq enforces max 3000 beams per call.
        # Send in batches so large ULS updates (which may produce tens of
        # thousands of changed beams) do not fail validation.
        _BATCH = 3000
        with FailOnError(fail_on_error):
            try:
                for i in range(0, len(beams), _BATCH):
                    self._post(
                        command="directional_invalidate",
                        json=RcacheDirectionalInvalidateReq(
                            beams=beams[i:i + _BATCH]).model_dump())
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
        headers = {}
        if self._api_key:
            headers["Authorization"] = f"Bearer {self._api_key}"
        try:
            resp = requests.post(
                f"{self._rcache_server_url}/{command}",
                json=json, headers=headers, timeout=30)
            resp.raise_for_status()
        except requests.RequestException as ex:
            error(f"Error sending '{command}' post to Request cache Server: "
                  f"{ex}")
