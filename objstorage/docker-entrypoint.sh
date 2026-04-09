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
    echo "AFC_OBJST_PORT = ${AFC_OBJST_PORT}"
    echo "AFC_OBJST_WORKERS = ${AFC_OBJST_WORKERS}"
    echo "AFC_OBJST_HIST_PORT = ${AFC_OBJST_HIST_PORT}"
    echo "AFC_OBJST_HIST_WORKERS = ${AFC_OBJST_HIST_WORKERS}"
    ;;
  "production")
    echo "Running production profile"
    AFC_MSGHND_LOG_LEVEL="info"
    ;;
  *)
    echo "Uknown profile"
    AFC_MSGHND_LOG_LEVEL="info"
    ;;
esac

# Background cleanup: when /storage exceeds AFC_OBJST_MAX_DISK_PCT (default 70%),
# remove the oldest task directories (responses/ and afc_config/) in batches
# until usage drops below the threshold. Runs every 5 minutes.
AFC_OBJST_MAX_DISK_PCT=${AFC_OBJST_MAX_DISK_PCT:-70}
AFC_OBJST_DIR=${AFC_OBJST_LOCAL_DIR:-/storage}
# POSIX-only tooling below: the image base (python:3.12-alpine) has busybox
# df/find/sh, which lack GNU 'df --output', 'find -printf' and 'read -d';
# the previous GNU forms failed silently (stderr discarded) and the cleanup
# never ran.
disk_used_pct() {
  df -P "${AFC_OBJST_DIR}" 2>/dev/null | awk 'NR==2 { gsub(/%/, "", $5); print $5 }'
}
(
  while true; do
    sleep 300
    USED_PCT=$(disk_used_pct)
    if [ -z "${USED_PCT}" ]; then
      echo "objst cleanup: WARNING: df probe failed for ${AFC_OBJST_DIR}; cleanup skipped" >&2
      continue
    fi
    if [ "${USED_PCT}" -ge "${AFC_OBJST_MAX_DISK_PCT}" ]; then
      echo "objst cleanup: disk ${USED_PCT}% >= threshold ${AFC_OBJST_MAX_DISK_PCT}%"
      for subdir in responses afc_config; do
        # Delete oldest dirs (by mtime) one at a time until under threshold.
        # ls -1tr = oldest first; entries are task-id/hash-named dirs
        # created by the storage app (no spaces or newlines).
        for name in $(ls -1tr "${AFC_OBJST_DIR}/${subdir}" 2>/dev/null); do
          dir="${AFC_OBJST_DIR}/${subdir}/${name}"
          [ -d "${dir}" ] || continue
          USED_PCT=$(disk_used_pct)
          [ "${USED_PCT:-0}" -lt "${AFC_OBJST_MAX_DISK_PCT}" ] && break
          rm -rf -- "${dir}"
          echo "objst cleanup: removed ${dir}"
        done
      done
    fi
  done
) &

gunicorn --workers ${AFC_OBJST_WORKERS} --worker-class gevent --bind 0.0.0.0:${AFC_OBJST_PORT} afcobjst:objst_app &
gunicorn --workers ${AFC_OBJST_HIST_WORKERS} --worker-class gevent --bind 0.0.0.0:${AFC_OBJST_HIST_PORT} afcobjst:hist_app &

sleep infinity
