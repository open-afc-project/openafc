#!/bin/bash
# Kafka startup wrapper: initialises canonical ALS topics and configures the
# broker with restricted topic management settings.
# The two-phase startup adds ~30 s to the container's first boot.
set -eu

ALS_PORT="${KAFKA_CLIENT_PORT:-9092}"
BROKER_PORT="${KAFKA_BROKER_PORT:-9093}"
PROTO="${KAFKA_CLIENT_SECURITY_PROTOCOL:-PLAINTEXT}"

# Fail-closed on PLAINTEXT: the broker must not accept unauthenticated
# produce/consume unless an explicit opt-in flag is set.  This mirrors the
# als_siphon consumer guard (ALS_SIPHON_ALLOW_PLAINTEXT_KAFKA).
# Set ALS_KAFKA_ALLOW_PLAINTEXT=true in the compose environment to use the
# default PLAINTEXT transport (acceptable on compose-internal networks).
if [ "${PROTO}" = "PLAINTEXT" ] && [ "${ALS_KAFKA_ALLOW_PLAINTEXT:-false}" != "true" ]; then
    echo "kafka-entrypoint: PLAINTEXT transport is disabled." \
         "Set ALS_KAFKA_ALLOW_PLAINTEXT=true to opt in," \
         "or set KAFKA_CLIENT_SECURITY_PROTOCOL to SSL/SASL_SSL/SASL_PLAINTEXT." >&2
    exit 1
fi

HOST="${KAFKA_ADVERTISED_HOST:-localhost}"
MSG_SIZE="${KAFKA_MAX_REQUEST_SIZE:-1048576}"
ALS_TOPIC="${AFC_ALS_TOPIC_NAME:-ALS}"
PREFIX="${AFC_JSON_TOPIC_PREFIX:-}"

# Build admin-client config so the readiness probe and topic-creation
# commands can authenticate when ${PROTO} is not PLAINTEXT.
CLIENT_CONFIG_ARGS=()
if [ "${PROTO}" != "PLAINTEXT" ]; then
    CLIENT_CONFIG="$(mktemp /tmp/kafka-admin-client.XXXXXX.properties)"
    chmod 600 "${CLIENT_CONFIG}"
    trap 'rm -f "${CLIENT_CONFIG:-}"' EXIT INT TERM
    {
        echo "security.protocol=${PROTO}"
        if [ "${PROTO#SASL_}" != "${PROTO}" ]; then
            echo "sasl.mechanism=${KAFKA_SASL_MECHANISM:-PLAIN}"
            if [ -n "${KAFKA_ADMIN_SASL_JAAS_CONFIG:-}" ]; then
                echo "sasl.jaas.config=${KAFKA_ADMIN_SASL_JAAS_CONFIG}"
            fi
        fi
        if [ "${PROTO%SSL}" != "${PROTO}" ]; then
            [ -n "${KAFKA_SSL_TRUSTSTORE_LOCATION:-}" ] && \
                echo "ssl.truststore.location=${KAFKA_SSL_TRUSTSTORE_LOCATION}"
            [ -n "${KAFKA_SSL_TRUSTSTORE_PASSWORD:-}" ] && \
                echo "ssl.truststore.password=${KAFKA_SSL_TRUSTSTORE_PASSWORD}"
            [ -n "${KAFKA_SSL_KEYSTORE_LOCATION:-}" ] && \
                echo "ssl.keystore.location=${KAFKA_SSL_KEYSTORE_LOCATION}"
            [ -n "${KAFKA_SSL_KEYSTORE_PASSWORD:-}" ] && \
                echo "ssl.keystore.password=${KAFKA_SSL_KEYSTORE_PASSWORD}"
        fi
    } > "${CLIENT_CONFIG}"
    CLIENT_CONFIG_ARGS=(--command-config "${CLIENT_CONFIG}")
fi

# Phase 1 — start Kafka briefly with topic management enabled so canonical
# topics can be created before the broker opens to producers/consumers.
env \
    KAFKA_AUTO_CREATE_TOPICS_ENABLE=true \
    KAFKA_MESSAGE_MAX_BYTES="${MSG_SIZE}" \
    KAFKA_MAX_REQUEST_SIZE="${MSG_SIZE}" \
    KAFKA_LISTENERS="PLAINTEXT://:${ALS_PORT},CONTROLLER://:${BROKER_PORT}" \
    KAFKA_LISTENER_SECURITY_PROTOCOL_MAP="CONTROLLER:${PROTO},PLAINTEXT:${PROTO}" \
    KAFKA_ADVERTISED_LISTENERS="PLAINTEXT://${HOST}:${ALS_PORT}" \
    KAFKA_CONTROLLER_QUORUM_VOTERS="1@localhost:${BROKER_PORT}" \
    /etc/kafka/docker/run &
INIT_PID=$!

# Wait for the broker to accept API requests (max 120 s).
WAITED=0
until /opt/kafka/bin/kafka-broker-api-versions.sh \
      --bootstrap-server "localhost:${ALS_PORT}" \
      "${CLIENT_CONFIG_ARGS[@]}" >/dev/null 2>&1; do
    sleep 2
    WAITED=$((WAITED + 2))
    if [ "${WAITED}" -ge 120 ]; then
        echo "kafka-entrypoint: broker did not start within 120 s" >&2
        kill "${INIT_PID}" 2>/dev/null || true
        exit 1
    fi
done

# Pre-create all canonical ALS topics.  --if-not-exists makes this idempotent
# so repeated container restarts are safe.
for TOPIC in \
    "${ALS_TOPIC}" \
    "${PREFIX}user_access" \
    "${PREFIX}fs_download" \
    "${PREFIX}afc_engine_crash" \
    "${PREFIX}rcache_update" \
    "${PREFIX}rcache_precomputation" \
    "${PREFIX}rcache_invalidation" \
    "${PREFIX}afc_config" \
    "${PREFIX}cert_db"; do
    /opt/kafka/bin/kafka-topics.sh \
        --bootstrap-server "localhost:${ALS_PORT}" \
        "${CLIENT_CONFIG_ARGS[@]}" \
        --create --if-not-exists \
        --topic "${TOPIC}" \
        --partitions 1 \
        --replication-factor 1 >/dev/null 2>&1 || true
done

# Phase 2 — stop the init Kafka.
kill "${INIT_PID}" 2>/dev/null || true
wait "${INIT_PID}" 2>/dev/null || true

# Phase 3 — run Kafka with topic management restricted to pre-created topics.
exec env \
    KAFKA_AUTO_CREATE_TOPICS_ENABLE=false \
    KAFKA_MESSAGE_MAX_BYTES="${MSG_SIZE}" \
    KAFKA_MAX_REQUEST_SIZE="${MSG_SIZE}" \
    KAFKA_LISTENERS="PLAINTEXT://:${ALS_PORT},CONTROLLER://:${BROKER_PORT}" \
    KAFKA_LISTENER_SECURITY_PROTOCOL_MAP="CONTROLLER:${PROTO},PLAINTEXT:${PROTO}" \
    KAFKA_ADVERTISED_LISTENERS="PLAINTEXT://${HOST}:${ALS_PORT}" \
    KAFKA_CONTROLLER_QUORUM_VOTERS="1@localhost:${BROKER_PORT}" \
    /etc/kafka/docker/run
