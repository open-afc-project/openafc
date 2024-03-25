#
# Copyright 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

import prometheus_client.multiprocess


def child_exit(server, worker):
    prometheus_client.multiprocess.mark_process_dead(worker.pid)
