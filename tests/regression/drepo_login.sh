#!/bin/bash
#
# Copyright  2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

set -x
docker_login() {
  aws ecr-public get-login-password --region us-east-1 | docker login --username AWS --password-stdin public.ecr.aws/w9v6y1o0
  aws ecr get-login-password --region us-east-1 | docker login --username AWS --password-stdin 110738915961.dkr.ecr.us-east-1.amazonaws.com
}

docker_login