#!/bin/sh
#
# Copyright (C) 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
AFC_DEVEL_ENV=${AFC_DEVEL_ENV:-production}
case "$AFC_DEVEL_ENV" in
  "devel")
    echo "Running debug profile" 
    ACCEPTOR_LOG_LEVEL="--log debug"
    BIN=nginx-debug
    apk add --update --no-cache bash
    ;;
  "production")
    echo "Running production profile"
    ACCEPTOR_LOG_LEVEL=
    BIN=nginx
    ;;
  *)
    echo "Uknown profile"
    ACCEPTOR_LOG_LEVEL=
    BIN=nginx
    ;;
esac

if [ ! -f /certificates/servers/server.key.pem ] || [ ! -f /certificates/servers/server.cert.pem ]; then
    echo "ERROR: TLS server certificates missing at /certificates/servers/server.key.pem and/or /certificates/servers/server.cert.pem. You must mount these."
    exit 1
fi

# Ensure an mTLS client-CA bundle is present so nginx can load
# ssl_client_certificate before acceptor.py installs the real, admin-signed
# bundle. Generate a fresh, unique, throwaway placeholder here instead of
# baking a static repo-committed dummy CA into the image: a shared static
# placeholder's private-key custody can never be verified from source, and
# every deployment would otherwise trust the SAME placeholder during its
# bootstrap window. No client certs are ever issued against this CA.
CLIENT_BUNDLE=/etc/nginx/certs/client.bundle.pem
if [ ! -f "$CLIENT_BUNDLE" ]; then
    BUNDLE_DIR=$(dirname "$CLIENT_BUNDLE")
    if [ -d "$BUNDLE_DIR" ]; then
        echo "WARNING: $CLIENT_BUNDLE not found; generating a fresh throwaway placeholder CA (no client certs issued against it). Install the real bundle via the admin API."
        WORK_CA=$(mktemp -d)
        openssl req -new -x509 -newkey rsa:2048 -nodes \
            -keyout "$WORK_CA/ca.key" -out "$CLIENT_BUNDLE" \
            -days 1 -subj "/CN=placeholder-no-trust" -sha256 2>/dev/null
        rm -rf "$WORK_CA"
    fi
fi

# Ensure a CRL file is present so nginx can load the ssl_crl directive.
# In production, distribute a CA-signed CRL alongside client.bundle.pem and
# regenerate it whenever a leaf cert is revoked.
for CRL_PATH in /etc/nginx/certs/client.crl.pem /certificates/clients/client.crl.pem; do
    if [ ! -f "$CRL_PATH" ]; then
        CRL_DIR=$(dirname "$CRL_PATH")
        if [ -d "$CRL_DIR" ]; then
            echo "WARNING: $CRL_PATH not found; generating empty placeholder CRL. Distribute a real CRL for production use."
            WORK_CRL=$(mktemp -d)
            openssl genrsa -out "$WORK_CRL/ca.key" 2048 2>/dev/null
            openssl req -new -x509 -key "$WORK_CRL/ca.key" -out "$WORK_CRL/ca.crt" \
                -days 1 -subj "/CN=placeholder-crl" -sha256 2>/dev/null
            mkdir -p "$WORK_CRL/db"
            touch "$WORK_CRL/db/index.txt"
            printf '1000\n' > "$WORK_CRL/db/crlnumber"
            printf '[ca]\ndefault_ca=X\n[X]\ndatabase=%s/db/index.txt\ncrlnumber=%s/db/crlnumber\ndefault_md=sha256\ndefault_crl_days=1\n' \
                "$WORK_CRL" "$WORK_CRL" > "$WORK_CRL/ca.cnf"
            openssl ca -gencrl -keyfile "$WORK_CRL/ca.key" -cert "$WORK_CRL/ca.crt" \
                -out "$CRL_PATH" -config "$WORK_CRL/ca.cnf" 2>/dev/null || true
            rm -rf "$WORK_CRL"
        fi
    fi
done

# nginx 20-envsubst-on-templates.sh only substitutes variables that
# are already in the environment (it builds a filter from `env | cut -d= -f1`).
# ${AFC_INTERNAL_TOKEN} in nginx.conf.template would otherwise be left as-is
# and nginx would fail with "unknown afc_internal_token variable".
# Export the token from the Docker secret file so envsubst picks it up.
if [ -z "${AFC_INTERNAL_TOKEN:-}" ] && [ -n "${AFC_INTERNAL_TOKEN_FILE:-}" ] \
   && [ -f "${AFC_INTERNAL_TOKEN_FILE}" ]; then
    AFC_INTERNAL_TOKEN=$(cat "${AFC_INTERNAL_TOKEN_FILE}")
    export AFC_INTERNAL_TOKEN
fi

# Export the dispatcher token from the Docker secret file so envsubst picks
# it up when processing nginx.conf.template (same pattern as AFC_INTERNAL_TOKEN).
if [ -z "${AFC_DISPATCHER_TOKEN:-}" ] && [ -n "${AFC_DISPATCHER_TOKEN_FILE:-}" ] \
   && [ -f "${AFC_DISPATCHER_TOKEN_FILE}" ]; then
    AFC_DISPATCHER_TOKEN=$(cat "${AFC_DISPATCHER_TOKEN_FILE}")
    export AFC_DISPATCHER_TOKEN
fi

/docker-entrypoint.sh $BIN -g "daemon off;" &

# Let nginx finish startup hooks and write /var/run/nginx.pid before acceptor
# runs `nginx -s reload` (avoids "open() nginx.pid failed").
i=0
while [ ! -f /var/run/nginx.pid ] && [ "$i" -lt 300 ]; do
  sleep 0.2
  i=$((i + 1))
done
if [ ! -f /var/run/nginx.pid ]; then
  echo "ERROR: nginx did not create /var/run/nginx.pid (timeout)."
  exit 1
fi

/wd/acceptor.py $ACCEPTOR_LOG_LEVEL --cmd run

exit $?
