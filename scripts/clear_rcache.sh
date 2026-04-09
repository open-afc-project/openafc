#!/bin/bash
#
# clear_rcache.sh — Invalidate the entire rcache before a fresh engine test run.
#
# Usage:
#   ./scripts/clear_rcache.sh                # from project root
#   ../scripts/clear_rcache.sh               # from local/ or prod/ subdirectory
#
# The script auto-detects COMPOSE_PROJECT_NAME from a .env file in the calling
# directory (same pattern as tests/create-session.sh), so running from local/
# gives brcm-afc-local-rcache-1 and from prod/ gives brcm-afc-prod-rcache-1.
#
# The rcache service runs inside the Docker network and is not exposed on a host
# port. This script reaches it via "docker exec" on the rcache container, which
# reads the API key from the secret file mounted inside the container at
# /run/secrets/RCACHE_API_KEY.
#
# To override the container name:
#   RCACHE_CONTAINER=my-rcache-1 ./scripts/clear_rcache.sh
#
# To call the rcache directly without Docker (e.g. in CI with an exposed port):
#   RCACHE_URL=http://localhost:8081 RCACHE_API_KEY=<key> ./scripts/clear_rcache.sh
#
set -euo pipefail

# Auto-detect COMPOSE_PROJECT_NAME from .env in the calling directory.
# This lets the script work from local/ or prod/ without requiring an explicit
# `export COMPOSE_PROJECT_NAME` — same pattern as tests/create-session.sh.
CALLING_DIR="$(pwd)"
if [ -z "${COMPOSE_PROJECT_NAME:-}" ] && [ -f "${CALLING_DIR}/.env" ]; then
    _cpn=$(grep '^COMPOSE_PROJECT_NAME=' "${CALLING_DIR}/.env" 2>/dev/null \
           | head -1 | cut -d= -f2 | tr -d "\"'")
    [ -n "$_cpn" ] && export COMPOSE_PROJECT_NAME="$_cpn"
fi

RCACHE_CONTAINER="${RCACHE_CONTAINER:-${COMPOSE_PROJECT_NAME:-brcm-afc}-rcache-1}"
RCACHE_PORT="${RCACHE_CLIENT_PORT:-8000}"

if [ -n "${RCACHE_URL:-}" ]; then
    # Direct (non-Docker) path: caller supplies RCACHE_URL and RCACHE_API_KEY
    if [ -z "${RCACHE_API_KEY:-}" ]; then
        echo "ERROR: RCACHE_URL is set but RCACHE_API_KEY is not set." >&2
        exit 1
    fi
    echo "Invalidating rcache at ${RCACHE_URL}/invalidate ..."
    # The API key is passed to curl via a config file on stdin (-K -), not as
    # an argv token: an argv-borne "Authorization: Bearer <key>" header would
    # be visible to any local user via /proc/<curl-pid>/cmdline (SUB-0138-66).
    HTTP_STATUS=$(curl -s -o /dev/null -w "%{http_code}" -K - <<CURLCFG
url = "${RCACHE_URL}/invalidate"
request = "POST"
header = "Authorization: Bearer ${RCACHE_API_KEY}"
header = "Content-Type: application/json"
data = "{\"ruleset_ids\": null}"
CURLCFG
)
else
    # Docker path: reach rcache via docker exec (reads its own secret file).
    # As above, the key is fed to curl via a config file on stdin, not argv,
    # so it never appears in the inner curl process's /proc/<pid>/cmdline.
    # NOTE: \$(...) and \"...\" are intentional — they are passed through the
    # outer shell as literals and evaluated by the inner sh -c shell inside
    # the container.
    echo "Invalidating rcache via container ${RCACHE_CONTAINER} ..."
    HTTP_STATUS=$(docker exec "${RCACHE_CONTAINER}" sh -c \
        "curl -s -o /dev/null -w '%{http_code}' -K - <<CURLCFG
url = \"http://localhost:${RCACHE_PORT}/invalidate\"
request = \"POST\"
header = \"Authorization: Bearer \$(cat /run/secrets/RCACHE_API_KEY)\"
header = \"Content-Type: application/json\"
data = \"{\\\"ruleset_ids\\\": null}\"
CURLCFG
")
fi

if [ "$HTTP_STATUS" -eq 200 ] || [ "$HTTP_STATUS" -eq 204 ]; then
    echo "rcache invalidated successfully (HTTP ${HTTP_STATUS})."
else
    echo "ERROR: rcache invalidation returned HTTP ${HTTP_STATUS}." >&2
    exit 1
fi
