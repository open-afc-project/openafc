# PostgreSQL Major-Version Upgrade Guide

**Scope**: upgrading the two AFC databases from PostgreSQL 14 to PostgreSQL 17  
**Affected services**: `ratdb` (AFC certification DB) and `bulk_postgres` (ALS logs/metrics DB)  
**Expected downtime**: 2–30 minutes depending on database size  
**Who should read this**: operators managing a self-hosted Open-AFC Docker deployment

---

## Why upgrade?

PostgreSQL 14 reaches **end-of-life on 9 November 2026** — no further security patches will be
released after that date. PostgreSQL 17 brings:

| Category | Improvement |
|----------|-------------|
| Security | EOL extended to Nov 2029; safe `search_path` defaults; new `MAINTAIN` permission |
| Performance | ~20× lower VACUUM memory; ~2× WAL throughput; streaming I/O; incremental backup |
| Operations | `pg_createsubscriber` for logical-replication standbys; `JSON_TABLE()`; smarter planner |

See [deferred-upgrades-plan.md](deferred-upgrades-plan.md) for the full justification.

---

## Is there a zero-downtime path?

For most Docker-based self-hosted deployments the answer is **no** — a dump-and-restore is the
safest and most reliable procedure.  A logical-replication approach
(stream changes from PG 14 → PG 17 in parallel, then do a brief cutover) can theoretically
reduce downtime to seconds but requires:

- manual configuration of `wal_level = logical` on PG 14,
- setting up a PG 17 instance alongside (different port or host), and
- a carefully timed `pg_createsubscriber` / cutover sequence.

That approach is operationally complex, error-prone, and harder to test.  We **recommend the
dump-and-restore path** below.  With a modest DB (< 1 GB), the total outage is under 5 minutes.

---

## Quick start (automated script)

A ready-made script handles all steps end-to-end:

```bash
# Dry-run first — prints every command that would be executed, touches nothing
./scripts/upgrade_pg_production.sh --dry-run

# Real upgrade (stop apps, dump PG 14, restore into PG 17, restart)
./scripts/upgrade_pg_production.sh
```

### Script options

| Option | Default | Description |
|--------|---------|-------------|
| `--dry-run` | off | Print commands only, make no changes |
| `--backup-dir DIR` | `/tmp/pg_upgrade_YYYYMMDD_HHmmSS` | Where SQL dumps are stored |
| `--project NAME` | `brcm-afc` | Docker Compose project name |
| `--compose FILE` | `./docker-compose.yaml` | Compose file path |
| `--skip-ratdb` | off | Skip ratdb, upgrade bulk_postgres only |
| `--skip-bulk` | off | Skip bulk_postgres, upgrade ratdb only |

The script:

1. Runs pre-flight checks (confirms PG 14 is running, Docker available)
2. Dumps both databases with `pg_dumpall`
3. Stops all application containers (databases stay running for the final dump)
4. Stops each database, removes the old data volume, starts PG 17, restores from dump
5. Runs `VACUUM ANALYZE` on both databases
6. Brings all containers back up
7. Prints the post-upgrade server version to confirm success
8. On any error, prints a full rollback procedure

---

## Step-by-step walkthrough

If you prefer to run each step manually or need to adapt the procedure:

### 1. Pre-flight

```bash
# Confirm both databases are running PG 14
docker exec brcm-afc-ratdb-1        psql -U postgres -tAc "SHOW server_version;"
docker exec brcm-afc-bulk_postgres-1 psql -U postgres -tAc "SHOW server_version;"

# Record DB sizes so you can estimate dump time
docker exec brcm-afc-ratdb-1        psql -U postgres -c "\l+"
docker exec brcm-afc-bulk_postgres-1 psql -U postgres -c "\l+"
```

### 2. Enable persistent volumes (if not already done)

> **Important**: by default the `docker-compose.yaml` has volumes commented out.
> If your ratdb or bulk_postgres data lives only inside the container (ephemeral), it
> will be lost when the container is replaced.  The dump-and-restore script handles this
> because data is captured in the SQL dump — but for ongoing persistence you should enable:

```yaml
# docker-compose.yaml (uncomment):
  ratdb:
    volumes:
      - ./pgdata:/mnt/nfs/psql/data

  bulk_postgres:
    volumes:
      - ./bulk_pgdata:/var/lib/postgresql/data
```

### 3. Back up both databases

```bash
mkdir -p ~/pg14_backup
docker exec brcm-afc-ratdb-1         pg_dumpall -U postgres > ~/pg14_backup/ratdb.sql
docker exec brcm-afc-bulk_postgres-1  pg_dumpall -U postgres > ~/pg14_backup/bulk_postgres.sql

# Confirm files are non-empty
ls -lh ~/pg14_backup/
```

### 4. Stop application containers (keep DBs up)

```bash
# Stop everything except ratdb and bulk_postgres
docker compose stop $(docker compose config --services | grep -v -E '^(ratdb|bulk_postgres)$')
```

### 5. Upgrade ratdb (postgres:14 → postgres:17)

```bash
# Stop DB
docker compose stop ratdb

# Optional: preserve old data volume
docker volume create brcm-afc_ratdb-data_pg14_backup
docker run --rm \
  -v brcm-afc_ratdb-data:/from:ro \
  -v brcm-afc_ratdb-data_pg14_backup:/to \
  busybox sh -c 'cp -a /from/. /to/'

# Remove the PG 14 data directory (PG 17 cannot read PG 14 data files)
docker volume rm brcm-afc_ratdb-data

# Update ratdb/Dockerfile: change  FROM postgres:14-alpine  →  FROM postgres:17-alpine
# Rebuild and start
docker compose up -d --build ratdb

# Wait for readiness
until docker exec brcm-afc-ratdb-1 pg_isready -U postgres -q; do sleep 2; done

# Restore
docker exec -i brcm-afc-ratdb-1 psql -U postgres < ~/pg14_backup/ratdb.sql

# Optimise statistics
docker exec brcm-afc-ratdb-1 vacuumdb -U postgres -a -z
```

### 6. Upgrade bulk_postgres (postgis/postgis:14-3.5 → postgis/postgis:17-3.5)

```bash
docker compose stop bulk_postgres

docker volume create brcm-afc_bulk_postgres-data_pg14_backup
docker run --rm \
  -v brcm-afc_bulk_postgres-data:/from:ro \
  -v brcm-afc_bulk_postgres-data_pg14_backup:/to \
  busybox sh -c 'cp -a /from/. /to/'

docker volume rm brcm-afc_bulk_postgres-data

# Update bulk_postgres/Dockerfile: FROM postgis/postgis:14-3.5 → postgis/postgis:17-3.5
docker compose up -d --build bulk_postgres

until docker exec brcm-afc-bulk_postgres-1 pg_isready -U postgres -q; do sleep 2; done

docker exec -i brcm-afc-bulk_postgres-1 psql -U postgres < ~/pg14_backup/bulk_postgres.sql

# Run the PostGIS extension upgrade one-shot service
docker compose run --rm bulk_postgis_upgrade

docker exec brcm-afc-bulk_postgres-1 vacuumdb -U postgres -a -z
```

### 7. Restart the full stack and verify

```bash
docker compose up -d

# Confirm versions
docker exec brcm-afc-ratdb-1        psql -U postgres -tAc "SHOW server_version;"  # expect 17.*
docker exec brcm-afc-bulk_postgres-1 psql -U postgres -tAc "SHOW server_version;" # expect 17.*

# Confirm PostGIS extension
docker exec brcm-afc-bulk_postgres-1 psql -U postgres -tAc \
    "SELECT extversion FROM pg_extension WHERE extname='postgis';"
```

### 8. Run the full test suite

```bash
# See .prompts/build_test.md for the full procedure; at minimum:
cd tests
python3 afc_tests.py --cmd run --addr localhost --port 5443 --prot https
```

---

## Rollback

If anything goes wrong the script prints a rollback procedure automatically.
Manual rollback:

```bash
# 1. Tear down
docker compose down

# 2a. Restore ratdb from volume backup (if you ran the copy step above)
docker volume rm brcm-afc_ratdb-data 2>/dev/null || true
docker run --rm \
  -v brcm-afc_ratdb-data_pg14_backup:/from:ro \
  -v brcm-afc_ratdb-data:/to \
  busybox sh -c 'cp -a /from/. /to/'

# 2b. Or restore from SQL dump with a fresh PG 14 container
#     (revert ratdb/Dockerfile to postgres:14-alpine, rebuild, then:)
docker compose up -d ratdb
docker exec -i brcm-afc-ratdb-1 psql -U postgres < ~/pg14_backup/ratdb.sql

# 3. Repeat analogous steps for bulk_postgres

# 4. Bring everything back up
docker compose up -d
```

---

## Cleanup after successful upgrade

Once the stack is confirmed healthy and you have run the test suite:

```bash
# Remove PG 14 backup volumes (optional, frees disk space)
docker volume rm brcm-afc_ratdb-data_pg14_backup        2>/dev/null || true
docker volume rm brcm-afc_bulk_postgres-data_pg14_backup 2>/dev/null || true

# Remove old SQL dumps (they may be large)
rm -rf ~/pg14_backup
```

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| `FATAL: database files appear to belong to PostgreSQL 14` | Old data volume not removed | `docker volume rm <volume_name>` then restart |
| Restore hangs / is very slow | Large ALS logs in bulk_postgres | Normal — let it finish; progress visible via `\watch 5 SELECT count(*) FROM ...` |
| `ERROR: extension "postgis" already exists` during restore | PostGIS CREATE in dump conflicts | Safe to ignore; `bulk_postgis_upgrade` one-shot will reconcile |
| `worker` container fails to start after upgrade | RabbitMQ `transient_nonexcl_queues` | Ensure `rabbitmq/rabbitmq.conf` contains `deprecated_features.permit.transient_nonexcl_queues = true` |
| Tests fail for `CA.AFCS.URS.5` | Known flaky test under load | Re-run individually to confirm it passes |

---

*Last updated: May 2026*
