# PostgreSQL Major-Version Upgrade Guide

**Scope**: upgrading the two AFC databases from PostgreSQL 14 to PostgreSQL 17  
**Affected services**: `ratdb` (AFC certification DB) and `bulk_postgres` (ALS logs/metrics DB)  
**Expected downtime**: 2–30 minutes depending on database size  
**Who should read this**: operators managing a self-hosted Open-AFC Docker deployment

---

## 1. Why upgrade?

PostgreSQL 14 reaches **end-of-life on 9 November 2026** — no further security patches will be
released after that date. PostgreSQL 17 brings:

| Category | Improvement |
|----------|-------------|
| Security | EOL extended to Nov 2029; safe `search_path` defaults; new `MAINTAIN` permission |
| Performance | ~20× lower VACUUM memory; ~2× WAL throughput; streaming I/O; incremental backup |
| Operations | `pg_createsubscriber` for logical-replication standbys; `JSON_TABLE()`; smarter planner |

See [deferred-upgrades-plan.md](deferred-upgrades-plan.md) for the full justification.

### Target Version Selection

This release keeps the database major at PostgreSQL 17 to limit deployment changes to the planned PG14-to-PG17 migration. PostgreSQL 17.12 is the current 17 minor release. PostgreSQL 18 is available, but its official container changes the data-directory volume layout; moving to it needs a separate compose and storage migration rehearsal. The bulk image moves to PostGIS 3.7, so the migration preflight rejects databases using the removed tiger geocoder or address-standardizer extensions.

The new target images have not yet passed the required staging migration rehearsal. Prior staging observations for PostgreSQL 17 / PostGIS 3.5 do not validate this target.

---

## 2. Release Dockerfile Pins

The release Dockerfiles are pinned to PostgreSQL 17.12 and PostGIS 17-3.7. Build and test these exact images before any production update.

### A. `ratdb/Dockerfile`

The PostgreSQL base image is:

```diff
FROM postgres:17.12-alpine@sha256:f9cf3e1f0066f0eb0c48707d864f0ac5941e9ff454a072077d97a4d278d07d73
```

### B. `bulk_postgres/Dockerfile`

The PostGIS base image is:

```diff
FROM postgis/postgis:17-3.7@sha256:0ea2759d63dae90e778f689bb65b5bc698929697ee188a1ca0aba104b3772264
```

---

## 3. Is there a zero-downtime path?

For most Docker-based self-hosted deployments the answer is **no** — a dump-and-restore is the
safest and most reliable procedure. A logical-replication approach
(stream changes from PG 14 → PG 17 in parallel, then do a brief cutover) can theoretically
reduce downtime to seconds but requires:

- manual configuration of `wal_level = logical` on PG 14,
- setting up a PG 17 instance alongside (different port or host), and
- a carefully timed `pg_createsubscriber` / cutover sequence.

That approach is operationally complex, error-prone, and harder to test. We **recommend the
dump-and-restore path** below. With database sizes under 2 GB, the outage window is typically under 5–10 minutes.

---

## 4. Automated Upgrade Script (`scripts/upgrade_pg_production.sh`)

The migration script automates the dump-and-restore, but it has not yet been
rehearsed against a fresh clone of the current production databases. Do not use
it for production until the exact release images, restore behavior, PostGIS
upgrade, and recovery procedure have passed a staging rehearsal.

```bash
# Dry-run first — validates compose files, container versions, and mounts without changes
./scripts/upgrade_pg_production.sh --dry-run

# Real upgrade (after exact staging rehearsal only)
./scripts/upgrade_pg_production.sh
```

### Script Capabilities & Hardening

* **Compose Overlay Auto-Detection**: Automatically detects and chains `./docker-compose.yaml`, `./docker-compose.brcm.yaml`, and `./docker-compose.staging.yaml` present in the current working directory, or accepts repeated `--compose FILE` arguments.
* **Host Bind Mount & Volume Support**: Detects both host bind mounts (`./pgdata`, `./bulk_pgdata` used in AWS production and staging) and named Docker volumes. Archives old PG 14 directories/volumes to `*_pg14_backup` and prepares fresh mode-`0700` directories for clean PostgreSQL 17 cluster initialization.
* **Target Image Pre-flight Verification**: Inspects target images defined in the compose files to ensure they resolve to PostgreSQL 17 before stopping containers or modifying directories.
* **Container Re-creation**: Uses `--force-recreate --no-deps` to ensure the newly tagged PostgreSQL 17 images are instantiated.
* **Role Ownership & Schema Hardening**: Automatically runs `postgis-upgrade.sh` for extension synchronization and realigns table ownership and schema privileges for application roles `bulk_rw` (read/write) and `bulk_ro` (read-only) across all databases (`rcache`, `ALS`, `fs_state`, `AFC_LOGS`, `grafana`).

### Script options

| Option | Default | Description |
|--------|---------|-------------|
| `--dry-run` | off | Print commands only, make no changes |
| `--backup-dir DIR` | fresh mode-`0700` dir under `/tmp` | Directory for SQL dumps |
| `--project NAME` | auto-detected from `.env`, else `brcm-afc` | Docker Compose project name |
| `--compose FILE` | auto-detected in CWD | Path to compose file (may be specified multiple times) |
| `--skip-ratdb` | off | Skip ratdb, upgrade bulk_postgres only |
| `--skip-bulk` | off | Skip bulk_postgres, upgrade ratdb only |

---

## 5. Next Steps for Production Upgrade

When upgrading the production deployment on `afc.broadcom.com` (`/opt/afc/wd`), follow this coordinated procedure:

### Phase 1: Build & Stage Production Images

1. **Update Dockerfiles**:
   Update `ratdb/Dockerfile` and `bulk_postgres/Dockerfile` to the pinned PostgreSQL 17 base images (as documented in Section 2).
2. **Build Release Images**:
   Build all images under the target release tag:
   ```bash
   ./tests/regression/build_imgs.sh /path/to/repo <RELEASE_TAG> 0
   ```
3. **Validate in Staging**:
   Stream images and test in the staging environment on AWS (`/opt/afc/work/<user>/staging_wd`) using `setup_staging.sh` and `control_staging.sh`.
4. **Push/Stream to Production Host**:
   Load or pull the release images on the AWS host (`afc.broadcom.com`) so they are present in the host Docker daemon prior to the maintenance window.

### Phase 2: Execute Production Database & Application Upgrade (Oneshot)

During the scheduled maintenance window on `afc`:

1. **Connect to Production Host**:
   ```bash
   ssh afc
   cd /opt/afc/wd
   ```
2. **Update Image Tag in Production `.env`**:
   Update `TAG=<RELEASE_TAG>` in `/opt/afc/wd/.env`.
3. **Execute Pre-flight Dry-Run**:
   ```bash
   ./scripts/upgrade_pg_production.sh --dry-run
   ```
   Verify that:
   - Current server versions are detected as PG 14.
   - Target images are verified as PostgreSQL 17 (`postgres (PostgreSQL) 17.x`).
   - Host bind mounts (`/opt/afc/wd/pgdata` and `/opt/afc/wd/bulk_pgdata`) are detected.
4. **Run Live Upgrade**:
   ```bash
   ./scripts/upgrade_pg_production.sh
   ```
   The script will:
   - Perform an online dump of `ratdb` and `bulk_postgres` to a mode-0700 backup directory.
   - Gracefully stop application containers.
   - Archive `/opt/afc/wd/pgdata` to `/opt/afc/wd/pgdata_pg14_backup` and `/opt/afc/wd/bulk_pgdata` to `/opt/afc/wd/bulk_pgdata_pg14_backup`.
   - Start PostgreSQL 17 containers and initialize clean clusters.
   - Restore the database dumps and realign superuser passwords from Docker secrets.
   - Execute `postgis-upgrade.sh` and align `bulk_rw` / `bulk_ro` table ownership and schema privileges.
   - Execute `VACUUM ANALYZE`.
   - Bring up all application containers with the new image tag.
   - Verify post-upgrade PostgreSQL versions and database lists.

### Phase 3: Post-Upgrade Verification

1. **Check Container Status**:
   ```bash
   docker ps --filter "name=wd-"
   ```
2. **Verify Database Versions**:
   ```bash
   docker exec wd-ratdb-1 psql -U postgres -tAc "SHOW server_version;"          # Expect 17.x
   docker exec wd-bulk_postgres-1 psql -U postgres -tAc "SHOW server_version;" # Expect 17.x
   docker exec wd-bulk_postgres-1 psql -U postgres -d rcache -tAc "SELECT PostGIS_Version();" # Expect 3.5.x
   ```
3. **Verify API Health & Smoke Tests**:
   ```bash
   curl -k https://afc.broadcom.com/fbrat/api/v1/health
   python3 scripts/unittest/live_checks.py -v
   ```
4. **Cleanup Post-Validation**:
   Once the production stack is verified healthy across traffic and logs:
   ```bash
   sudo rm -rf /opt/afc/wd/pgdata_pg14_backup
   sudo rm -rf /opt/afc/wd/bulk_pgdata_pg14_backup
   ```

---

## 6. Manual Walkthrough (Reference)

If executing steps individually or adapting for a custom environment:

### Step 1: Pre-flight Checks
```bash
docker exec wd-ratdb-1 psql -U postgres -tAc "SHOW server_version;"
docker exec wd-bulk_postgres-1 psql -U postgres -tAc "SHOW server_version;"
```

### Step 2: Back Up Both Databases
```bash
BACKUP_DIR=$(mktemp -d -t pg_backup_XXXXXXXX)
chmod 700 "$BACKUP_DIR"
docker exec wd-ratdb-1 pg_dumpall -U postgres > "$BACKUP_DIR/ratdb.sql"
docker exec wd-bulk_postgres-1 pg_dumpall -U postgres > "$BACKUP_DIR/bulk_postgres.sql"
```

### Step 3: Stop Application Containers
```bash
docker compose -f docker-compose.yaml -f docker-compose.brcm.yaml stop \
  $(docker compose -f docker-compose.yaml -f docker-compose.brcm.yaml config --services | grep -v -E '^(ratdb|bulk_postgres)$')
```

### Step 4: Upgrade `ratdb`
```bash
docker compose -f docker-compose.yaml -f docker-compose.brcm.yaml stop ratdb

# Host bind mount preservation (e.g. ./pgdata):
sudo mv ./pgdata ./pgdata_pg14_backup
mkdir -m 700 ./pgdata

# Start new PG 17 container
docker compose -f docker-compose.yaml -f docker-compose.brcm.yaml up -d --force-recreate --no-deps ratdb

# Wait for initdb readiness
until docker exec wd-ratdb-1 pg_isready -U postgres -q; do sleep 2; done

# Restore SQL dump
docker exec -i wd-ratdb-1 psql -U postgres < "$BACKUP_DIR/ratdb.sql"

# Re-align password and vacuum
docker exec -i wd-ratdb-1 psql -U postgres -c "ALTER USER postgres PASSWORD '$(cat ./secrets/RATDB_PASSWORD)';"
docker exec wd-ratdb-1 vacuumdb -U postgres -a -z
```

### Step 5: Upgrade `bulk_postgres`
```bash
docker compose -f docker-compose.yaml -f docker-compose.brcm.yaml stop bulk_postgres

# Host bind mount preservation (e.g. ./bulk_pgdata):
sudo mv ./bulk_pgdata ./bulk_pgdata_pg14_backup
mkdir -m 700 ./bulk_pgdata

# Start new PG 17 container
docker compose -f docker-compose.yaml -f docker-compose.brcm.yaml up -d --force-recreate --no-deps bulk_postgres

until docker exec wd-bulk_postgres-1 pg_isready -U postgres -q; do sleep 2; done

# Restore SQL dump
docker exec -i wd-bulk_postgres-1 psql -U postgres < "$BACKUP_DIR/bulk_postgres.sql"

# Re-align superuser password
docker exec -i wd-bulk_postgres-1 psql -U postgres -c "ALTER USER postgres PASSWORD '$(cat ./secrets/BULK_POSTGRES_PASSWORD)';"

# Upgrade PostGIS extension SQL
docker exec -e PGPASSWORD="$(cat ./secrets/BULK_POSTGRES_PASSWORD)" -e PGCHANNELBINDING=disable \
  wd-bulk_postgres-1 /usr/local/bin/postgis-upgrade.sh

# Realign bulk_rw and bulk_ro permissions
for db in rcache ALS fs_state AFC_LOGS grafana; do
  docker exec -i wd-bulk_postgres-1 psql -U postgres -d "$db" << 'EOF'
DO $body$
DECLARE r RECORD;
BEGIN
  FOR r IN (SELECT tablename FROM pg_tables WHERE schemaname = 'public' AND tablename NOT IN ('spatial_ref_sys', 'geography_columns', 'geometry_columns')) LOOP
    EXECUTE 'ALTER TABLE public.' || quote_ident(r.tablename) || ' OWNER TO bulk_rw';
  END LOOP;
END $body$;
GRANT ALL ON SCHEMA public TO bulk_rw;
GRANT ALL ON ALL TABLES IN SCHEMA public TO bulk_rw;
GRANT ALL ON ALL SEQUENCES IN SCHEMA public TO bulk_rw;
GRANT SELECT ON ALL TABLES IN SCHEMA public TO bulk_ro;
GRANT SELECT ON ALL SEQUENCES IN SCHEMA public TO bulk_ro;
GRANT USAGE ON SCHEMA public TO bulk_ro;
EOF
done

docker exec wd-bulk_postgres-1 vacuumdb -U postgres -a -z
```

### Step 6: Restart Stack & Validate
```bash
docker compose -f docker-compose.yaml -f docker-compose.brcm.yaml up -d
```

---

## 7. Rollback Procedure

If the upgrade encounters an unrecoverable failure:

1. Stop all containers:
   ```bash
   docker compose -f docker-compose.yaml -f docker-compose.brcm.yaml down
   ```
2. Restore preserved data directories:
   ```bash
   sudo rm -rf ./pgdata && sudo mv ./pgdata_pg14_backup ./pgdata
   sudo rm -rf ./bulk_pgdata && sudo mv ./bulk_pgdata_pg14_backup ./bulk_pgdata
   ```
3. Revert image tags in `.env` to the previous release tag.
4. Restart the stack:
   ```bash
   docker compose -f docker-compose.yaml -f docker-compose.brcm.yaml up -d
   ```

---

*Last updated: October 2026*
