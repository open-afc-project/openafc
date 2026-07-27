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
(
  while true; do
    sleep 300
    USED_PCT=$(df --output=pcent "${AFC_OBJST_DIR}" 2>/dev/null \
               | tail -1 | tr -d ' %')
    if [ "${USED_PCT:-0}" -ge "${AFC_OBJST_MAX_DISK_PCT}" ]; then
      echo "objst cleanup: disk ${USED_PCT}% >= threshold ${AFC_OBJST_MAX_DISK_PCT}%"
      for subdir in responses afc_config; do
        # Delete oldest dirs (by mtime) one at a time until under threshold
        find "${AFC_OBJST_DIR}/${subdir}" -mindepth 1 -maxdepth 1 -type d \
             -printf '%T@ %p\0' 2>/dev/null \
          | sort -zn \
          | while IFS= read -r -d '' entry; do
              dir="${entry#* }"
              USED_PCT=$(df --output=pcent "${AFC_OBJST_DIR}" 2>/dev/null \
                         | tail -1 | tr -d ' %')
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
