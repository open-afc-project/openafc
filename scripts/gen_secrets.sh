#!/usr/bin/env bash
# Generate random secrets for an OpenAFC deployment overlay.
#
# Creates missing secret files in <overlay>/secrets/.  By default, existing
# files are left untouched (idempotent).  Pass --rotate-token to regenerate
# AFC_INTERNAL_TOKEN even when it already exists, giving a fresh token on
# every deployment.
#
# Usage:
#   ./scripts/gen_secrets.sh [--rotate-token] [--rotate-all] <overlay_dir>
#
#   --rotate-token   Regenerate AFC_INTERNAL_TOKEN even if it exists.
#   --rotate-all     Regenerate ALL random secrets (NOT CAPTCHA/OIDC/mail).
#   overlay_dir      Overlay directory containing secrets/ subdirectory
#                    (e.g. local/, prod/, avgo/).  Defaults to current dir.
#
# Secrets that are NOT auto-generated (empty placeholder files are created):
#   CAPTCHA_SECRET, CAPTCHA_SITEKEY   – Google reCAPTCHA API credentials
#   GOOGLE_APIKEY                     – Google Maps API key
#   OIDC_CLIENT_ID, OIDC_CLIENT_SECRET – OIDC provider credentials
#   MAIL_PASSWORD                     – SMTP password
# Fill these in manually to enable the respective features.
#
# All other secrets are 64-hex-char (256-bit) random values.
#
# Docker secrets must exist on the host BEFORE docker compose up reads the
# secrets: block.  Run this script once before the first deployment, and
# optionally with --rotate-token before each subsequent deployment to rotate
# the AFC internal token.

set -euo pipefail

ROTATE_TOKEN=false
ROTATE_ALL=false
OVERLAY_DIR="."

for arg in "$@"; do
    case "$arg" in
        --rotate-token) ROTATE_TOKEN=true ;;
        --rotate-all)   ROTATE_ALL=true; ROTATE_TOKEN=true ;;
        -*) echo "Unknown flag: $arg" >&2; exit 1 ;;
        *)  OVERLAY_DIR="$arg" ;;
    esac
done

SECRETS_DIR="$(cd "$OVERLAY_DIR" && pwd)/secrets"

if [[ ! -d "$SECRETS_DIR" ]]; then
    echo "Creating $SECRETS_DIR ..."
    mkdir -p "$SECRETS_DIR"
fi

# Permissions: only owner can read secrets
chmod 700 "$SECRETS_DIR"

rand256() {
    python3 -c "import secrets; print(secrets.token_hex(32))"
}

write_secret() {
    local name="$1"
    local value="$2"
    local path="$SECRETS_DIR/$name"
    printf '%s' "$value" > "$path"
    # 644: world-readable, but the secrets/ directory itself has 700 so host
    # users without directory execute permission cannot reach these files.
    # Container processes (non-root fbrat uid) need world-read to access
    # secrets via the bind-mount at /run/secrets/ inside the container.
    chmod 644 "$path"
    echo "  CREATED  $name"
}

gen_if_missing() {
    local name="$1"
    local path="$SECRETS_DIR/$name"
    if [[ -f "$path" ]]; then
        echo "  existing $name (skipped)"
    else
        write_secret "$name" "$(rand256)"
    fi
}

gen_or_rotate() {
    local name="$1"
    local force="$2"
    local path="$SECRETS_DIR/$name"
    if [[ "$force" == "true" ]] || [[ ! -f "$path" ]]; then
        write_secret "$name" "$(rand256)"
    else
        echo "  existing $name (skipped)"
    fi
}

echo "Generating secrets in: $SECRETS_DIR"
echo "  (rotate-token=$ROTATE_TOKEN, rotate-all=$ROTATE_ALL)"
echo ""

# ── Inter-service token (can rotate on every deployment) ─────────────────────
gen_or_rotate AFC_INTERNAL_TOKEN "$ROTATE_TOKEN"
# Dispatcher-only token injected by nginx for AP mTLS attestation headers.
# Mounted only into dispatcher, rat_server, msghnd, and afcserver — NOT into
# other AFC_INTERNAL_TOKEN holders. Rotate together with AFC_INTERNAL_TOKEN.
gen_or_rotate AFC_DISPATCHER_TOKEN "$ROTATE_TOKEN"

# ── Random secrets (rotate-all or create-if-missing) ─────────────────────────
gen_or_rotate BROKER_PWD         "$ROTATE_ALL"
gen_or_rotate RCACHE_RMQ_PWD     "$ROTATE_ALL"
gen_or_rotate FLASK_SECRET_KEY   "$ROTATE_ALL"
# Independent from FLASK_SECRET_KEY (session signing) so a SECRET_KEY leak
# does not also strip the pepper protecting every stored password hash
# (SUB-0138-75). See src/ratapi/ratapi/app.py's SECURITY_PASSWORD_SALT setup.
gen_or_rotate SECURITY_PASSWORD_SALT "$ROTATE_ALL"
gen_or_rotate OBJST_API_KEY      "$ROTATE_ALL"
gen_or_rotate RCACHE_API_KEY     "$ROTATE_ALL"
gen_or_rotate DISPATCHER_BUNDLE_HMAC_KEY "$ROTATE_ALL"

# ── Database passwords (only generated once; changing requires DB migration) ──
gen_if_missing RATDB_PASSWORD
# DB_CREATOR_PASSWORD_RATDB points to the same file (docker-compose.yaml), so
# no separate secret file is needed.

# Single shared password for all bulk_postgres connections.  All services that
# connect to bulk_postgres (rcache, ALS, Grafana, ULS, CreateDb) use the same
# postgres superuser credential (per-service roles are a tracked improvement).
# All docker-compose.yaml bulk_postgres secrets alias this file.
gen_if_missing BULK_POSTGRES_PASSWORD
gen_if_missing BULK_POSTGRES_RW_PASSWORD
gen_if_missing BULK_POSTGRES_RO_PASSWORD
gen_if_missing GRAFANA_ADMIN_PASSWORD

# ── External credentials (empty placeholder created; fill in a real value) ────
# These features are disabled when the file is empty (no CAPTCHA, no Maps, etc.).
# The files must exist on the host before `docker compose up` reads the secrets: block.
placeholder() {
    local name="$1"
    local path="$SECRETS_DIR/$name"
    if [[ -f "$path" ]]; then
        echo "  existing $name (manual)"
    else
        touch "$path"
        chmod 600 "$path"
        echo "  PLACEHOLDER $name  ← empty; fill in a real value to enable this feature"
    fi
}

for name in CAPTCHA_SECRET CAPTCHA_SITEKEY GOOGLE_APIKEY \
            OIDC_CLIENT_ID OIDC_CLIENT_SECRET MAIL_PASSWORD; do
    placeholder "$name"
done

echo ""
echo "Done. Secrets are in: $SECRETS_DIR"
echo ""
echo "If you ran --rotate-token, restart the overlay to pick up the new tokens:"
echo "  docker compose ... up -d --pull never --no-deps dispatcher afcserver rat_server msghnd"
