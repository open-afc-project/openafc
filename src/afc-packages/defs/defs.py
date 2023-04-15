#
# Copyright © 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# These individual bits corresponds to RUNTIME_OPT_... bits in AfcManager.cpp
# Please keep definitions synchronous
RNTM_OPT_DBG = 1
RNTM_OPT_GUI = 2
RNTM_OPT_AFCENGINE_HTTP_IO = 4
RNTM_OPT_NOCACHE = 8
RNTM_OPT_SLOW_DBG = 16

RNTM_OPT_NODBG_NOGUI = 0
RNTM_OPT_DBG_NOGUI = RNTM_OPT_DBG
RNTM_OPT_NODBG_GUI = RNTM_OPT_GUI
RNTM_OPT_DBG_GUI = (RNTM_OPT_DBG | RNTM_OPT_GUI)

