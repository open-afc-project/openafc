#
# Copyright © 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
import ratapi

app = ratapi.create_app(
    config_override={
        'APPLICATION_ROOT': '/fbrat',
    }
)
