# Custom docker-socket-proxy image.
# Replaces the upstream haproxy.cfg.template with one that adds an explicit
# deny rule for filesystem-read endpoints before the CONTAINERS allow rule.
# This avoids modifying the running container's config at startup.
#
# Copyright (C) 2024 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program

FROM tecnativa/docker-socket-proxy:v0.4.2@sha256:1f3a6f303320723d199d2316a3e82b2e2685d86c275d5e3deeaf182573b47476

COPY socket-proxy/haproxy.cfg.template /usr/local/etc/haproxy/haproxy.cfg.template
