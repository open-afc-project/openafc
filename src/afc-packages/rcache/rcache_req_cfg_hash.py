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
from typing import Any, Dict


class RequestConfigHash:
    """ Computes and holds request/config hash and other collaterals

    Public attributes:
    req_cfg_hash -- Request/config hash
    cfg_str      -- AFC Config as string
    cfg_hash     -- Config hash (None if not requested)
    """
    def __init__(self, req_dict: Dict[str, Any],
                 afc_config_dict: Dict[str, Any],
                 compute_config_hash: bool = False) -> None:
        """ Constructor

        Arguments:
        req_dict            -- Individual AFC Request in dictionary form
        afc_config_dict     -- AFC Config in dictionary form
        compute_config_hash -- True to also compute config hash
        """
        md5 = hashlib.md5()
        self.cfg_str = json.dumps(afc_config_dict, sort_keys=True)
        md5.update(self.cfg_str.encode("utf-8"))
        self.cfg_hash = md5.hexdigest() if compute_config_hash else None
        md5.update(
            json.dumps(
                {k: v for k, v in req_dict.items() if k != "requestId"},
                sort_keys=True).encode('utf-8'))
        self.req_cfg_hash = md5.hexdigest()
