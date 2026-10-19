""" Computes request/config hash and other collaterals """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
# pylint: disable=too-few-public-methods

import hashlib
import json
from typing import Any, Dict, Optional


class RequestConfigHash:
    """ Computes and holds request/config hash and other collaterals

    Public attributes:
    req_cfg_hash -- Request/config hash
    cfg_str      -- AFC Config as string
    cfg_hash     -- Config hash (None if not requested)
    """

    def __init__(self, req_dict: Dict[str, Any],
                 afc_config_dict: Dict[str, Any],
                 compute_config_hash: bool = False,
                 runtime_opt: Optional[int] = None) -> None:
        """ Constructor

        Arguments:
        req_dict            -- Individual AFC Request in dictionary form
        afc_config_dict     -- AFC Config in dictionary form
        compute_config_hash -- True to also compute config hash
        runtime_opt         -- DB-derived runtime options (e.g. RNTM_OPT_CERT_ID
                               for indoor-certified devices).  Must be included in
                               the hash so a cert reclassified indoor→outdoor
                               cannot continue to receive the cached indoor grant.
        """
        h = hashlib.sha256()
        self.cfg_str = json.dumps(afc_config_dict, sort_keys=True)
        h.update(self.cfg_str.encode("utf-8"))
        self.cfg_hash = h.hexdigest() if compute_config_hash else None
        h.update(
            json.dumps(
                {k: v for k, v in req_dict.items() if k != "requestId"},
                sort_keys=True).encode('utf-8'))
        # Mix in the DB-derived runtime options so indoor-vs-outdoor cert
        # status changes are not masked by a stale cache entry.
        if runtime_opt is not None:
            h.update(runtime_opt.to_bytes(4, "little"))
        self.req_cfg_hash = h.hexdigest()
