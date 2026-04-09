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

# Authorization (deny-by-default).  auto.create.topics.enable=false alone
# does not restrict the AdminClient CreateTopics RPC, so without an
# authorizer any client passing transport has full admin rights on every
# topic.  StandardAuthorizer is enabled in both startup phases below.
AUTHZ_CLASS="org.apache.kafka.metadata.authorizer.StandardAuthorizer"
# ';'-separated principals granted least-privilege PRODUCE rights
# (Write+Describe, no topic Read) on the canonical topics.  Topic Read
# is scoped below to SIPHON_PRINCIPALS only: producer-side services
# must not be able to consume the ALS regulatory audit stream or other
# services' log topics.  The default matches the PLAINTEXT
# compose-internal opt-in (every client is User:ANONYMOUS); set to the
# real client principals when SASL/SSL is enabled.
CLIENT_PRINCIPALS="${ALS_KAFKA_CLIENT_PRINCIPALS:-User:ANONYMOUS}"
# ';'-separated principals allowed to CONSUME as the als_siphon consumer
# group.  Kafka Group:Read authorizes JoinGroup/SyncGroup/Heartbeat/
# OffsetCommit (mutating), so under multi-principal SASL set this to the
# siphon's principal only: producer-side services must not be able to
# join the siphon's group or tamper with its committed offsets.
# Defaults to CLIENT_PRINCIPALS for single-principal deployments.
SIPHON_PRINCIPALS="${ALS_KAFKA_SIPHON_PRINCIPALS:-${CLIENT_PRINCIPALS}}"
# Consumer group used by als_siphon (its group.id default is "ALS").
SIPHON_GROUP="${ALS_KAFKA_SIPHON_GROUP:-ALS}"
# ';'-separated principals allowed to PRODUCE to the main ALS audit topic
# (AFC request/response/config records).  The siphon attributes audit rows
# to the afcServer claimed in the message body and keeps the first-arrived
# response, so Write on this topic is audit-forgery capability: under
# multi-principal SASL set this to the afc_server/msghnd/rat_server
# principals only.  Defaults to CLIENT_PRINCIPALS for single-principal
# deployments.
ALS_PRODUCER_PRINCIPALS="${ALS_KAFKA_ALS_PRODUCER_PRINCIPALS:-${CLIENT_PRINCIPALS}}"
# Principal the broker itself presents on the CONTROLLER listener (needs
# ClusterAction for broker registration/heartbeats).
BROKER_PRINCIPAL="${ALS_KAFKA_BROKER_PRINCIPAL:-User:ANONYMOUS}"
# Optional operator super-user principal.  REQUIRED under SASL/SSL so the
# entrypoint's own admin commands stay authorized in both phases.
ADMIN_PRINCIPAL="${ALS_KAFKA_ADMIN_PRINCIPAL:-}"

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
# Both listeners are bound to loopback so the auto-create window is never
# reachable from the network, and User:ANONYMOUS is a super-user only while
# nothing but this entrypoint can connect (loopback bind + localhost
# advertisement).
env \
    KAFKA_AUTO_CREATE_TOPICS_ENABLE=true \
    KAFKA_MESSAGE_MAX_BYTES="${MSG_SIZE}" \
    KAFKA_MAX_REQUEST_SIZE="${MSG_SIZE}" \
    KAFKA_AUTHORIZER_CLASS_NAME="${AUTHZ_CLASS}" \
    KAFKA_ALLOW_EVERYONE_IF_NO_ACL_FOUND=false \
    KAFKA_SUPER_USERS="User:ANONYMOUS${ADMIN_PRINCIPAL:+;${ADMIN_PRINCIPAL}}" \
    KAFKA_LISTENERS="PLAINTEXT://127.0.0.1:${ALS_PORT},CONTROLLER://127.0.0.1:${BROKER_PORT}" \
    KAFKA_LISTENER_SECURITY_PROTOCOL_MAP="CONTROLLER:${PROTO},PLAINTEXT:${PROTO}" \
    KAFKA_ADVERTISED_LISTENERS="PLAINTEXT://localhost:${ALS_PORT}" \
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
CANONICAL_TOPICS=(
    "${ALS_TOPIC}"
    "${PREFIX}user_access"
    "${PREFIX}fs_download"
    "${PREFIX}afc_engine_crash"
    "${PREFIX}rcache_update"
    "${PREFIX}rcache_precomputation"
    "${PREFIX}rcache_invalidation"
    "${PREFIX}afc_config"
    "${PREFIX}cert_db"
)
for TOPIC in "${CANONICAL_TOPICS[@]}"; do
    /opt/kafka/bin/kafka-topics.sh \
        --bootstrap-server "localhost:${ALS_PORT}" \
        "${CLIENT_CONFIG_ARGS[@]}" \
        --create --if-not-exists \
        --topic "${TOPIC}" \
        --partitions 1 \
        --replication-factor 1 >/dev/null 2>&1 || true
done

# Bootstrap least-privilege ACLs.  StandardAuthorizer persists them in the
# KRaft metadata log, so they carry over to phase 3 and later restarts.
# Failures are deliberately NOT suppressed: an unauthorized broker must not
# come up looking healthy (fail closed, set -e).  --add is idempotent.
/opt/kafka/bin/kafka-acls.sh \
    --bootstrap-server "localhost:${ALS_PORT}" \
    "${CLIENT_CONFIG_ARGS[@]}" \
    --add --allow-principal "${BROKER_PRINCIPAL}" \
    --operation ClusterAction --cluster >/dev/null
IFS=';' read -r -a ACL_PRINCIPALS <<< "${CLIENT_PRINCIPALS}"
for PRINCIPAL in "${ACL_PRINCIPALS[@]}"; do
    [ -n "${PRINCIPAL}" ] || continue
    for TOPIC in "${CANONICAL_TOPICS[@]}"; do
        if [ "${TOPIC}" = "${ALS_TOPIC}" ]; then
            # Write on the main ALS audit topic is granted only to
            # ALS_PRODUCER_PRINCIPALS (loop below), not to every client
            # principal: any writer can forge the audit record (the
            # siphon trusts the body-claimed afcServer and keeps the
            # first-arrived response).
            TOPIC_OPS=(--operation Read --operation Describe)
            # Revoke the pre-fix uniform Write persisted by earlier
            # boots (StandardAuthorizer keeps ACLs in the KRaft
            # metadata log).  No-op on fresh installs; re-added below
            # for the producer principals.
            /opt/kafka/bin/kafka-acls.sh \
                --bootstrap-server "localhost:${ALS_PORT}" \
                "${CLIENT_CONFIG_ARGS[@]}" \
                --remove --force --allow-principal "${PRINCIPAL}" \
                --operation Write --topic "${TOPIC}" >/dev/null
        else
            TOPIC_OPS=(--operation Read --operation Write --operation Describe)
        fi
        /opt/kafka/bin/kafka-acls.sh \
            --bootstrap-server "localhost:${ALS_PORT}" \
            "${CLIENT_CONFIG_ARGS[@]}" \
            --add --allow-principal "${PRINCIPAL}" \
            "${TOPIC_OPS[@]}" \
            --topic "${TOPIC}" >/dev/null
        # Revoke the pre-fix flat topic Read persisted by earlier boots
        # (StandardAuthorizer keeps ACLs in the KRaft metadata log; no-op
        # on fresh installs).  Topic Read is re-granted below to
        # SIPHON_PRINCIPALS only: a producer-side principal could
        # otherwise consume the ALS audit stream, learn live bundle keys
        # and pre-empt genuine responses with forged audit records.
        /opt/kafka/bin/kafka-acls.sh \
            --bootstrap-server "localhost:${ALS_PORT}" \
            "${CLIENT_CONFIG_ARGS[@]}" \
            --remove --force --allow-principal "${PRINCIPAL}" \
            --operation Read \
            --topic "${TOPIC}" >/dev/null
    done
    # Non-mutating group Describe so observability clients (kafka_ui on
    # the kafka-broker-ui network) keep working.  Group Read is NOT
    # granted on '*': it authorizes JoinGroup/OffsetCommit and is scoped
    # below to the siphon principals on the siphon's own group only.
    /opt/kafka/bin/kafka-acls.sh \
        --bootstrap-server "localhost:${ALS_PORT}" \
        "${CLIENT_CONFIG_ARGS[@]}" \
        --add --allow-principal "${PRINCIPAL}" \
        --operation Describe --group '*' >/dev/null
    # Revoke the pre-fix wildcard group Read persisted by earlier boots
    # (StandardAuthorizer keeps ACLs in the KRaft metadata log, so the
    # old over-grant would otherwise survive this upgrade).  No-op on
    # fresh installs.
    /opt/kafka/bin/kafka-acls.sh \
        --bootstrap-server "localhost:${ALS_PORT}" \
        "${CLIENT_CONFIG_ARGS[@]}" \
        --remove --force --allow-principal "${PRINCIPAL}" \
        --operation Read --group '*' >/dev/null
    /opt/kafka/bin/kafka-acls.sh \
        --bootstrap-server "localhost:${ALS_PORT}" \
        "${CLIENT_CONFIG_ARGS[@]}" \
        --add --allow-principal "${PRINCIPAL}" \
        --operation Describe --cluster >/dev/null
done
# Consumer-side ACLs for the als_siphon consumers, scoped to the siphon
# principals only: topic Read (+Describe, in case the siphon principal
# is not also a producer principal) on the canonical topics, and group
# Read (JoinGroup/SyncGroup/Heartbeat/OffsetCommit) on the siphon's own
# consumer group.  This runs AFTER the producer loop above, so in
# single-principal deployments (SIPHON_PRINCIPALS defaults to
# CLIENT_PRINCIPALS) the shared principal keeps its consume rights.
IFS=';' read -r -a SIPHON_ACL_PRINCIPALS <<< "${SIPHON_PRINCIPALS}"
for PRINCIPAL in "${SIPHON_ACL_PRINCIPALS[@]}"; do
    [ -n "${PRINCIPAL}" ] || continue
    for TOPIC in "${CANONICAL_TOPICS[@]}"; do
        /opt/kafka/bin/kafka-acls.sh \
            --bootstrap-server "localhost:${ALS_PORT}" \
            "${CLIENT_CONFIG_ARGS[@]}" \
            --add --allow-principal "${PRINCIPAL}" \
            --operation Read --operation Describe \
            --topic "${TOPIC}" >/dev/null
    done
    /opt/kafka/bin/kafka-acls.sh \
        --bootstrap-server "localhost:${ALS_PORT}" \
        "${CLIENT_CONFIG_ARGS[@]}" \
        --add --allow-principal "${PRINCIPAL}" \
        --operation Read --group "${SIPHON_GROUP}" >/dev/null
done
# Produce rights on the main ALS audit topic: scoped to the ALS producer
# principals so a compromised non-producer service credential cannot
# pre-empt the genuine afc_server response with a forged audit record
# (als_siphon.py AlsMessageBundle.update keeps the first-arrived response
# and trusts the body-claimed afcServer identity).
IFS=';' read -r -a ALS_PRODUCER_ACL_PRINCIPALS <<< "${ALS_PRODUCER_PRINCIPALS}"
for PRINCIPAL in "${ALS_PRODUCER_ACL_PRINCIPALS[@]}"; do
    [ -n "${PRINCIPAL}" ] || continue
    /opt/kafka/bin/kafka-acls.sh \
        --bootstrap-server "localhost:${ALS_PORT}" \
        "${CLIENT_CONFIG_ARGS[@]}" \
        --add --allow-principal "${PRINCIPAL}" \
        --operation Write --topic "${ALS_TOPIC}" >/dev/null
done

# Phase 2 — stop the init Kafka.
kill "${INIT_PID}" 2>/dev/null || true
wait "${INIT_PID}" 2>/dev/null || true

# Phase 3 — run Kafka with topic management restricted to pre-created topics
# and deny-by-default authorization (ACLs bootstrapped in phase 1).
# User:ANONYMOUS is no longer a super-user from this point on: clients are
# limited to the ACL-granted produce/consume on canonical topics; the
# CreateTopics/DeleteTopics/AlterConfigs RPCs are denied.
# The CONTROLLER listener stays on loopback (as in phase 1): the quorum
# voter list is 1@localhost, so only the broker's own loopback traffic ever
# needs it; a wildcard bind would expose ClusterAction controller RPCs
# (granted to BROKER_PRINCIPAL, User:ANONYMOUS by default) to every network
# peer under PLAINTEXT. Only the client listener needs the wildcard bind.
exec env \
    KAFKA_AUTO_CREATE_TOPICS_ENABLE=false \
    KAFKA_MESSAGE_MAX_BYTES="${MSG_SIZE}" \
    KAFKA_MAX_REQUEST_SIZE="${MSG_SIZE}" \
    KAFKA_AUTHORIZER_CLASS_NAME="${AUTHZ_CLASS}" \
    KAFKA_ALLOW_EVERYONE_IF_NO_ACL_FOUND=false \
    KAFKA_SUPER_USERS="${ADMIN_PRINCIPAL}" \
    KAFKA_LISTENERS="PLAINTEXT://:${ALS_PORT},CONTROLLER://127.0.0.1:${BROKER_PORT}" \
    KAFKA_LISTENER_SECURITY_PROTOCOL_MAP="CONTROLLER:${PROTO},PLAINTEXT:${PROTO}" \
    KAFKA_ADVERTISED_LISTENERS="PLAINTEXT://${HOST}:${ALS_PORT}" \
    KAFKA_CONTROLLER_QUORUM_VOTERS="1@localhost:${BROKER_PORT}" \
    /etc/kafka/docker/run
