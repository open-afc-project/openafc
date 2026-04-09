#!/usr/bin/env python3
"""Generate or update a SHA-256 hash manifest for ULS SQLite databases.

The manifest is a JSON object mapping bare filename → lowercase SHA-256 hex
digest.  It is consumed by uls_service.py when ULS_HASH_MANIFEST_URL or
ULS_HASH_MANIFEST_FILE is set.

Typical use-cases
-----------------
1.  Run on a SEPARATE trusted machine / CI system (not the uls_downloader host)
    that independently downloads the same FCC/ISED raw data and recomputes the
    SQLite.  Publish the resulting manifest to a read-only store (GitHub, S3,
    etc.) before the uls_downloader's next validation cycle.

2.  Run manually to bootstrap the manifest from a known-good set of SQLite
    files that already live on the NFS share.

NOTE: Running this script on the *same host* that produces the SQLite files
provides a second validation layer but NOT an independent trust anchor.
The manifest is only as trustworthy as the host that generates it.
See the deployment security documentation for additional controls.

Usage examples
--------------
  # Add (or update) one file in the manifest:
  python3 generate_hash_manifest.py --manifest manifest.json \
      /opt/afc/databases/rat_transfer/ULS_Database/FS_LATEST.sqlite3

  # Add all SQLite files in a directory (skip symlinks):
  python3 generate_hash_manifest.py --manifest manifest.json \
      --dir /opt/afc/databases/rat_transfer/ULS_Database/

  # Prune entries whose files no longer exist locally, then write:
  python3 generate_hash_manifest.py --manifest manifest.json \
      --dir /opt/afc/databases/rat_transfer/ULS_Database/ --prune

  # Dry-run: print what would change without writing:
  python3 generate_hash_manifest.py --manifest manifest.json \
      --dir /opt/afc/databases/rat_transfer/ULS_Database/ --dry_run

  # Print the computed hash for a single file (no manifest update):
  python3 generate_hash_manifest.py \
      /opt/afc/databases/rat_transfer/ULS_Database/FS_LATEST.sqlite3
"""

# Copyright (C) 2026 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

import argparse
import glob
import hashlib
import hmac
import json
import logging
import os
import sys
import tempfile
import time
from typing import Dict, List, Optional


def sha256_file(path: str, chunk: int = 65536) -> str:
    """Return the lowercase SHA-256 hex digest of *path* (streamed)."""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(chunk), b""):
            h.update(block)
    return h.hexdigest()


def load_manifest(path: str) -> Dict[str, str]:
    """Load existing manifest from *path*, or return empty dict."""
    if not os.path.isfile(path):
        return {}
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    if not isinstance(data, dict):
        raise ValueError(f"Manifest '{path}' is not a JSON object")
    return data


def save_manifest(manifest: Dict[str, str], path: str) -> None:
    """Atomically write *manifest* to *path* (sorted keys)."""
    tmp_fd, tmp_path = tempfile.mkstemp(
        dir=os.path.dirname(os.path.abspath(path)),
        suffix=".tmp")
    try:
        with os.fdopen(tmp_fd, "w", encoding="utf-8") as f:
            json.dump(dict(sorted(manifest.items())), f, indent=2)
            f.write("\n")
        os.replace(tmp_path, path)
    except Exception:
        try:
            os.unlink(tmp_path)
        except OSError:
            pass
        raise


def hash_files(paths: List[str], manifest: Dict[str, str],
               dry_run: bool) -> int:
    """Hash each path and update *manifest*.

    Returns number of entries that were new or changed.
    """
    changed = 0
    for path in paths:
        if os.path.islink(path):
            logging.info(f"Skipping symlink: {path}")
            continue
        if not os.path.isfile(path):
            logging.warning(f"Not a regular file, skipping: {path}")
            continue
        key = os.path.basename(path)
        logging.info(f"Hashing {path} ...")
        digest = sha256_file(path)
        existing = manifest.get(key)
        if existing == digest:
            logging.info(f"  {key}: unchanged ({digest})")
        else:
            verb = "new" if existing is None else "CHANGED"
            logging.info(f"  {key}: {verb}  {digest}")
            if not dry_run:
                manifest[key] = digest
            changed += 1
    return changed


def prune_manifest(manifest: Dict[str, str], db_dir: str,
                   dry_run: bool) -> int:
    """Remove entries whose files no longer exist in *db_dir*.

    Returns number of pruned entries.
    """
    pruned = 0
    for key in list(manifest.keys()):
        if not os.path.isfile(os.path.join(db_dir, key)):
            logging.info(f"Pruning missing entry: {key}")
            if not dry_run:
                del manifest[key]
            pruned += 1
    return pruned


def main(argv: List[str]) -> int:
    logging.basicConfig(
        level=logging.INFO,
        format="%(levelname)s: %(message)s")

    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument(
        "files", nargs="*", metavar="SQLITE_FILE",
        help="One or more SQLite files to hash and add to the manifest")
    ap.add_argument(
        "--manifest", metavar="MANIFEST_JSON", required=False,
        help="Path to the manifest JSON file to read/write. "
             "If omitted, only prints computed hashes without writing")
    ap.add_argument(
        "--dir", metavar="DB_DIR",
        help="Hash all non-symlink *.sqlite3 files in this directory")
    ap.add_argument(
        "--prune", action="store_true",
        help="Remove manifest entries whose files are absent from --dir")
    ap.add_argument(
        "--dry_run", action="store_true",
        help="Compute and log changes but do not write the manifest")
    ap.add_argument(
        "--hmac_key", metavar="HMAC_KEY",
        help="DEPRECATED and REJECTED: passing the HMAC key on argv exposes "
             "it via /proc/<pid>/cmdline and CI step logs. Use "
             "--hmac_key_file or the ULS_HASH_MANIFEST_HMAC_KEY environment "
             "variable instead.")
    ap.add_argument(
        "--hmac_key_file", metavar="HMAC_KEY_FILE",
        help="File containing the HMAC-SHA256 key for signing the manifest "
             "(e.g. a Docker secret, mode 0600). When provided, writes a "
             "companion '<manifest>.hmac' file containing the HMAC-SHA256 "
             "hex digest of the manifest content. The key must match "
             "ULS_HASH_MANIFEST_HMAC_KEY in uls_service; it may also be "
             "supplied via that environment variable.")

    args = ap.parse_args(argv)

    if args.hmac_key:
        ap.error(
            "--hmac_key is no longer accepted: passing the signing key on "
            "argv exposes it via /proc/<pid>/cmdline and CI step logs. Use "
            "--hmac_key_file or set ULS_HASH_MANIFEST_HMAC_KEY in the "
            "environment instead.")

    if not args.files and not args.dir:
        ap.error("Provide at least one SQLITE_FILE argument or --dir")

    # Collect files to hash
    paths: List[str] = list(args.files)
    if args.dir:
        if not os.path.isdir(args.dir):
            logging.error(f"Directory not found: {args.dir}")
            return 1
        paths += sorted(glob.glob(os.path.join(args.dir, "*.sqlite3")))
    if not paths:
        logging.warning("No SQLite files found")
        return 0

    # No manifest path → just print hashes and exit
    if not args.manifest:
        for path in paths:
            if os.path.islink(path):
                continue
            if not os.path.isfile(path):
                continue
            digest = sha256_file(path)
            print(f"{digest}  {os.path.basename(path)}")
        return 0

    manifest = load_manifest(args.manifest)

    if args.prune:
        db_dir = args.dir or os.path.dirname(
            os.path.abspath(args.files[0])) if args.files else "."
        pruned = prune_manifest(manifest, db_dir, args.dry_run)
        if pruned:
            logging.info(f"Pruned {pruned} entries")

    changed = hash_files(paths, manifest, args.dry_run)

    if changed == 0 and not args.prune:
        logging.info("Nothing changed in manifest")
        return 0

    if args.dry_run:
        logging.info(
            f"[dry_run] {changed} entries would change; manifest not written")
        return 0

    # Bind an issued-at epoch into the manifest so it is covered by the
    # HMAC below; the verifier rejects replayed older manifests (CWE-294).
    manifest["_iat"] = str(int(time.time()))
    save_manifest(manifest, args.manifest)
    logging.info(
        f"Manifest written to '{args.manifest}' "
        f"({len(manifest)} total entries, {changed} changed)")

    # If an HMAC key is provided, write the companion .hmac file
    # so uls_service can verify manifest integrity independently of its transport.
    hmac_key: Optional[str] = os.environ.get("ULS_HASH_MANIFEST_HMAC_KEY")
    if args.hmac_key_file:
        try:
            with open(args.hmac_key_file, encoding="utf-8") as kf:
                hmac_key = kf.read().strip()
        except OSError as ex:
            logging.error(f"Cannot read HMAC key file '{args.hmac_key_file}': {ex}")
            return 1
    if hmac_key:
        with open(args.manifest, "rb") as f:
            manifest_bytes = f.read()
        sig = hmac.new(
            hmac_key.encode("utf-8"), manifest_bytes, hashlib.sha256
        ).hexdigest()
        hmac_path = args.manifest + ".hmac"
        with open(hmac_path, "w", encoding="ascii") as hf:
            hf.write(sig + "\n")
        logging.info(f"HMAC-SHA256 signature written to '{hmac_path}'")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
