#!/usr/bin/env bash
# update_uls_manifest.sh — Regenerate the ULS SHA-256 hash manifest and
# optionally publish it to a production server via SCP.
#
# Run this on a SEPARATE machine from the production uls_downloader host
# (different host, different credentials) after each new ULS SQLite file
# is produced.  The production uls_downloader reads the manifest via
# ULS_HASH_MANIFEST_FILE (local path inside the container) or
# ULS_HASH_MANIFEST_URL (HTTPS endpoint).
#
# Usage:
#   ./update_uls_manifest.sh --db-dir DIR --manifest FILE [OPTIONS]
#
# Required:
#   --db-dir   DIR     Directory containing *.sqlite3 files
#                      (e.g. /nfs/rat_transfer/ULS_Database/ on this machine)
#   --manifest FILE    Local path to write manifest.json
#
# Optional:
#   --scp-dest DEST    SCP destination for the manifest after generation, e.g.
#                      user@prod-host:/path/to/uls-hash-manifest.json
#                      If omitted, SCP step is skipped.
#   --hmac-key-file F  Path to the ULS_HASH_MANIFEST_HMAC_KEY secret file.
#                      Signs the manifest; the .hmac companion is published
#                      alongside it.  The downloader rejects unauthenticated
#                      manifests, so production publishes MUST sign.
#   --prune            Remove manifest entries for SQLite files that no longer
#                      exist in --db-dir
#   --dry-run          Show what would change without writing
#   -h, --help         Show this help
#
# Cron example (run every 4 hours, publish to production server):
#   0 */4 * * *  /opt/afc/brcm-afc/uls/update_uls_manifest.sh \
#       --db-dir  /nfs/rat_transfer/ULS_Database/ \
#       --manifest /nfs/rat_transfer/uls-hash-manifest.json \
#       --scp-dest afc@prod-server:/opt/afc/databases/rat_transfer/uls-hash-manifest.json \
#       --prune >> /var/log/uls_manifest.log 2>&1
#
# Production uls_downloader configuration (.env or docker-compose override):
#   ULS_HASH_MANIFEST_FILE=/rat_transfer/uls-hash-manifest.json
#   (or ULS_HASH_MANIFEST_URL if serving the manifest over HTTPS)
#
# See uls/README.md §SHA-256 hash manifest check for the full trust model
# and uls/generate_hash_manifest.py for direct CLI usage.

set -euo pipefail

# ---------- argument parsing ---------------------------------------------------
DB_DIR=""
MANIFEST=""
SCP_DEST=""
HMAC_KEY_FILE=""
PRUNE=""
DRY_RUN=""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GENERATE="${SCRIPT_DIR}/generate_hash_manifest.py"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --db-dir)   DB_DIR="$2";    shift 2 ;;
        --manifest) MANIFEST="$2";  shift 2 ;;
        --scp-dest) SCP_DEST="$2";  shift 2 ;;
        --hmac-key-file) HMAC_KEY_FILE="$2"; shift 2 ;;
        --prune)    PRUNE="--prune"; shift   ;;
        --dry-run)  DRY_RUN="--dry_run"; shift ;;
        -h|--help)
            sed -n 's/^# //p; s/^#$//p' "$0" | head -50
            exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

if [[ -z "${DB_DIR}" || -z "${MANIFEST}" ]]; then
    echo "ERROR: --db-dir and --manifest are required." >&2
    echo "       Run '$0 --help' for usage." >&2
    exit 1
fi

# ---------- main ---------------------------------------------------------------
echo "[$(date -u +%FT%TZ)] Updating ULS hash manifest"
echo "  DB_DIR   : ${DB_DIR}"
echo "  MANIFEST : ${MANIFEST}"

if [[ -n "${HMAC_KEY_FILE}" ]]; then
    python3 "${GENERATE}" \
        --manifest "${MANIFEST}" \
        --dir "${DB_DIR}" \
        --hmac_key_file "${HMAC_KEY_FILE}" \
        ${PRUNE} ${DRY_RUN}
else
    echo "WARNING: --hmac-key-file not given — manifest will be unsigned" >&2
    echo "         and the production uls_downloader will reject it." >&2
    python3 "${GENERATE}" \
        --manifest "${MANIFEST}" \
        --dir "${DB_DIR}" \
        ${PRUNE} ${DRY_RUN}
fi

if [[ -z "${DRY_RUN}" && -n "${SCP_DEST}" ]]; then
    echo "[$(date -u +%FT%TZ)] Publishing manifest to ${SCP_DEST}"
    scp "${MANIFEST}" "${SCP_DEST}"
    # Publish the HMAC companion with the manifest (also written when the
    # ULS_HASH_MANIFEST_HMAC_KEY env var is set instead of --hmac-key-file).
    if [[ -f "${MANIFEST}.hmac" ]]; then
        scp "${MANIFEST}.hmac" "${SCP_DEST}.hmac"
    fi
    echo "[$(date -u +%FT%TZ)] Published OK"
fi

echo "[$(date -u +%FT%TZ)] Done"
