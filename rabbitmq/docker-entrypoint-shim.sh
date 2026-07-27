#!/bin/sh
# Generate definitions.json from template at container startup,
# substituting BROKER_USER, BROKER_PWD, and RCACHE_RMQ_PWD from environment.
# Supports file-based Docker secrets via BROKER_PWD_FILE / RCACHE_RMQ_PWD_FILE.
# This avoids baking the broker password into the image layer.
set -eu

TEMPLATE=/etc/rabbitmq/definitions.json.template
TARGET=/etc/rabbitmq/definitions.json

read_secret_file() {
    _file="$1"
    if [ -f "$_file" ]; then
        cat "$_file" | tr -d '\n'
    fi
}

if [ -n "${BROKER_PWD_FILE:-}" ] && [ -f "${BROKER_PWD_FILE}" ]; then
    BROKER_PWD=$(read_secret_file "$BROKER_PWD_FILE")
    export BROKER_PWD
fi

if [ -n "${RCACHE_RMQ_PWD_FILE:-}" ] && [ -f "${RCACHE_RMQ_PWD_FILE}" ]; then
    RCACHE_RMQ_PWD=$(read_secret_file "$RCACHE_RMQ_PWD_FILE")
    export RCACHE_RMQ_PWD
fi

# Require non-empty credentials before writing definitions so that a missing
# or empty Docker secret does not produce a broker with an empty password.
: "${BROKER_PWD:?BROKER_PWD must be non-empty — set BROKER_PWD_FILE or BROKER_PWD}"
: "${RCACHE_RMQ_PWD:?RCACHE_RMQ_PWD must be non-empty — set RCACHE_RMQ_PWD_FILE or RCACHE_RMQ_PWD}"

if [ -f "$TEMPLATE" ]; then
    envsubst '${BROKER_USER} ${BROKER_PWD} ${RCACHE_RMQ_PWD}' \
        < "$TEMPLATE" > "$TARGET"
    # Transfer ownership to the rabbitmq user (the shim runs as root) so the
    # process can read the file, then restrict to owner-only.
    chown rabbitmq "$TARGET"
    chmod 600 "$TARGET"
fi

# Pre-create .erlang.cookie before the prelaunch duplicate-node check runs.
# Without this, the prelaunch Erlang node may fail to read the cookie on the
# first boot because the official docker-entrypoint.sh chown step has not yet
# run. The shim runs as root so it can always create the file with correct
# rabbitmq ownership.
_COOKIE=/var/lib/rabbitmq/.erlang.cookie
if [ ! -f "$_COOKIE" ]; then
    mkdir -p /var/lib/rabbitmq
    # openssl is provided by the rabbitmq base image (TLS library).
    COOKIE_VAL=$(openssl rand -hex 20)
    (umask 177 && printf '%s' "$COOKIE_VAL" > "$_COOKIE")
    chown rabbitmq:rabbitmq "$_COOKIE"
fi

# Start RabbitMQ in background, wait for it to be ready, delete the default
# guest account, then exec into the normal server process.
# load_definitions (management plugin) merges users and cannot delete pre-
# existing ones; rabbitmqctl is the only reliable deletion path.
_delete_guest() {
    _retries=30
    while [ $_retries -gt 0 ]; do
        if rabbitmqctl list_users >/dev/null 2>&1; then
            rabbitmqctl delete_user guest 2>/dev/null || true
            return 0
        fi
        sleep 1
        _retries=$((_retries - 1))
    done
}

# Run delete_guest in background after starting the server.
(_delete_guest) &

exec docker-entrypoint.sh "$@"

# definitions.json is intentionally not deleted here because RabbitMQ reads it
# at startup (RABBITMQ_DEFAULT_DEFINITIONS_FILE). The chmod 600 above limits
# read access to the rabbitmq process owner only.
