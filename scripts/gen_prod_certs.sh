#!/usr/bin/env bash
# Generate TLS/mTLS certificates for an OpenAFC deployment overlay.
#
# Creates under <outdir>/:
#   certs/ca/ca_key.pem              — CA private key  (OFFLINE — NOT mounted into dispatcher)
#   certs/clients/ca_crt.pem         — CA certificate
#   certs/clients/ap_client_key.pem  — DEMO/TEST shared AP client key (mTLS)
#   certs/clients/ap_client_crt.pem  — DEMO/TEST shared AP client cert (mTLS)
#   certs/clients/client.bundle.pem  — CA bundle nginx loads for mTLS
#   certs/server/server.key.pem      — nginx TLS server private key
#   certs/server/server.cert.pem     — nginx TLS server certificate
#   certs/server/server.bundle.pem   — server cert + CA chain
#
# All certs are signed by the same self-signed CA (10-year validity).
# For production, replace these with certs from your organisation's CA.
#
# IMPORTANT: This script generates ONE shared ap_client key/cert pair for the
# entire AP fleet.  If any single AP device credential is exposed, the entire
# fleet mTLS key must be revoked and reissued simultaneously.
# For production, EACH AP must receive an individually-issued certificate
# (unique CN / SAN per device) signed by the deployment CA so that individual
# device certificates can be revoked without affecting the rest of the fleet.
# This script is suitable only for local testing / reference; do NOT distribute
# the generated ap_client_key.pem to multiple devices in production.
#
# ca_key.pem is written to certs/ca/ (NOT certs/clients/) so it is
#           NOT included in the dispatcher bind-mount (./dispatcher/certs/clients).
#           Only ca_crt.pem (the CA certificate) needs to be in certs/clients/.
#
# All temp files (CSR, ext config) use a private mktemp directory so
#           an unprivileged local user cannot race the extfile writes in /tmp.
#
# Usage:
#   ./scripts/gen_prod_certs.sh [outdir]
#
#   outdir  Optional output directory (default: current working directory).
#           All cert subdirectories are created inside it.
#
# Examples:
#   # From project root, into the 'avgo' overlay subfolder:
#   ./scripts/gen_prod_certs.sh avgo
#
#   # From inside the overlay subfolder (output goes to ./certs/...):
#   cd avgo && ../scripts/gen_prod_certs.sh
#
#   # From project root, into current directory:
#   ./scripts/gen_prod_certs.sh

set -euo pipefail

OUTDIR="${1:-.}"
# Resolve to absolute path (create dir first so cd works)
mkdir -p "$OUTDIR"
OUTDIR="$(cd "$OUTDIR" && pwd)"

# CA private key goes into certs/ca/ (offline dir), NOT certs/clients/
CA_DIR="$OUTDIR/certs/ca"
CLIENT_DIR="$OUTDIR/certs/clients"
SERVER_DIR="$OUTDIR/certs/server"

mkdir -p "$CA_DIR" "$CLIENT_DIR" "$SERVER_DIR"

# Use a private per-invocation temp directory (mode 700) instead of /tmp
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

echo "[1/4] Generating CA (4096-bit RSA, 10 years) ..."
openssl genrsa -out "$CA_DIR/ca_key.pem" 4096 2>/dev/null
chmod 600 "$CA_DIR/ca_key.pem"
openssl req -new -x509 \
    -key "$CA_DIR/ca_key.pem" \
    -out "$CLIENT_DIR/ca_crt.pem" \
    -days 3650 \
    -subj "/C=US/ST=California/L=San Jose/O=OpenAFC/OU=Test/CN=OpenAFC Test CA" \
    -sha256

echo "[2/4] Generating AP client certificate (shared)..."
openssl genrsa -out "$CLIENT_DIR/ap_client_key.pem" 4096 2>/dev/null
chmod 600 "$CLIENT_DIR/ap_client_key.pem"
openssl req -new \
    -key "$CLIENT_DIR/ap_client_key.pem" \
    -out "$WORK/ap_client.csr" \
    -subj "/C=US/ST=California/L=San Jose/O=OpenAFC/OU=Test AP/CN=afc-test-ap-client" \
    -sha256
cat > "$WORK/ap_client_ext.cnf" << 'EOF'
authorityKeyIdentifier=keyid,issuer
basicConstraints = CA:FALSE
extendedKeyUsage=clientAuth
keyUsage = critical, digitalSignature, keyEncipherment
subjectKeyIdentifier=hash
EOF
openssl x509 -req \
    -in "$WORK/ap_client.csr" \
    -CA "$CLIENT_DIR/ca_crt.pem" \
    -CAkey "$CA_DIR/ca_key.pem" \
    -CAcreateserial \
    -out "$CLIENT_DIR/ap_client_crt.pem" \
    -days 3650 -extfile "$WORK/ap_client_ext.cnf" -sha256

echo "[3/4] Building nginx client CA bundle + empty CRL..."
cp "$CLIENT_DIR/ca_crt.pem" "$CLIENT_DIR/client.bundle.pem"

# Generate an empty CRL signed by the deployment CA so nginx can load
# ssl_crl from day-one.  Re-run this step (or use 'openssl ca -revoke') when
# a leaf cert needs to be revoked, then distribute the new CRL to nginx.
mkdir -p "$WORK/ca_db"
touch "$WORK/ca_db/index.txt"
echo '1000' > "$WORK/ca_db/crlnumber"
cat > "$WORK/ca.cnf" << EOF
[ ca ]
default_ca = CA_default
[ CA_default ]
database       = $WORK/ca_db/index.txt
crlnumber      = $WORK/ca_db/crlnumber
default_md     = sha256
default_crl_days = 30
[ crl_ext ]
authorityKeyIdentifier=keyid:always
EOF
openssl ca -gencrl \
    -keyfile "$CA_DIR/ca_key.pem" \
    -cert    "$CLIENT_DIR/ca_crt.pem" \
    -out     "$CLIENT_DIR/client.crl.pem" \
    -config  "$WORK/ca.cnf" 2>/dev/null
chmod 644 "$CLIENT_DIR/client.crl.pem"
echo "  Empty CRL written to $CLIENT_DIR/client.crl.pem"
echo "  Redistribute this file to all dispatcher cert mounts after revoking any leaf cert."

echo "[4/4] Generating nginx server certificate..."
openssl genrsa -out "$SERVER_DIR/server.key.pem" 4096 2>/dev/null
chmod 600 "$SERVER_DIR/server.key.pem"
openssl req -new \
    -key "$SERVER_DIR/server.key.pem" \
    -out "$WORK/server.csr" \
    -subj "/C=US/ST=California/L=San Jose/O=OpenAFC/OU=Test/CN=localhost" \
    -sha256
cat > "$WORK/server_ext.cnf" << 'EOF'
authorityKeyIdentifier=keyid,issuer
basicConstraints = CA:FALSE
extendedKeyUsage=serverAuth
keyUsage = critical, digitalSignature, keyEncipherment
subjectAltName = DNS:localhost, IP:127.0.0.1
subjectKeyIdentifier=hash
EOF
openssl x509 -req \
    -in "$WORK/server.csr" \
    -CA "$CLIENT_DIR/ca_crt.pem" \
    -CAkey "$CA_DIR/ca_key.pem" \
    -CAcreateserial \
    -out "$SERVER_DIR/server.cert.pem" \
    -days 3650 -extfile "$WORK/server_ext.cnf" -sha256
cat "$SERVER_DIR/server.cert.pem" "$CLIENT_DIR/ca_crt.pem" \
    > "$SERVER_DIR/server.bundle.pem"

echo ""
echo "Done. Verification:"
openssl verify -CAfile "$CLIENT_DIR/ca_crt.pem" "$CLIENT_DIR/ap_client_crt.pem"
openssl verify -CAfile "$CLIENT_DIR/ca_crt.pem" "$SERVER_DIR/server.cert.pem"

echo ""
echo "NOTE: The CA private key is in $CA_DIR/ca_key.pem"
echo "  Do NOT mount $CA_DIR into any container — only $CLIENT_DIR needs to be"
echo "  bind-mounted into the dispatcher (as ./dispatcher/certs/clients:/etc/nginx/certs)."
echo "  Keep the CA key offline (or in a secrets manager) after initial cert issuance."
