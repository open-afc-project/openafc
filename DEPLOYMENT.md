This work is licensed under the OpenAFC Project License, a copy of which is
included with this software program.

# OpenAFC Deployment Guide

This guide walks an operator through a complete, production-ready deployment of
OpenAFC from a release download. It covers environment configuration, secret
generation, TLS certificate setup, starting the stack, first-time initialisation,
deployment validation, and ongoing operations.

For a quick orientation to the codebase and contribution workflow, see
[`README.md`](README.md).  
For a concise security reference (credentials, rotation, mTLS), see
[`SECURITY.md`](SECURITY.md).

---

## Table of Contents

1. [Prerequisites](#1-prerequisites)
2. [Get the Source](#2-get-the-source)
3. [Prepare Databases](#3-prepare-databases)
4. [Configure `.env`](#4-configure-env)
5. [Generate TLS Certificates](#5-generate-tls-certificates)
6. [Prepare Secrets Files](#6-prepare-secrets-files)
7. [Start the Stack](#7-start-the-stack)
8. [First-time Initialisation](#8-first-time-initialisation)
9. [Validate the Deployment](#9-validate-the-deployment)
10. [Performance Testing](#10-performance-testing)
11. [Ongoing Operations](#11-ongoing-operations)

---

## 1. Prerequisites

| Requirement | Minimum version | Notes |
|---|---|---|
| Docker Engine | 24+ | [Install guide](https://docs.docker.com/engine/install/) |
| Docker Compose plugin | 2.20+ | Ships with Docker Desktop; `docker compose version` |
| openssl | 1.1.1+ | For certificate generation |
| Python 3 | 3.9+ | For secret generation one-liners |
| Disk space for databases | ~600 GB | See [Prepare Databases](#3-prepare-databases) |

---

## 2. Get the Source

Clone the repository or download a release tarball:

```bash
git clone https://github.com/open-afc-project/openafc.git
cd openafc
```

---

## 3. Prepare Databases

OpenAFC requires several large static databases (terrain, land-cover, ULS, ITU
data, etc.). See [`database_readme.md`](database_readme.md) for the full list of
required datasets, where to download each one, and how to post-process them.

Mount all databases under a single host directory (e.g. `/opt/afc/databases/rat_transfer`)
and set `VOL_H_DB` in `.env` to that path (see [Step 4](#4-configure-env)).

The `worker` container runs as the internal `fbrat` user (uid/gid **1003**).
The database directories must be readable and writable by that uid.
If your host directories are already world-accessible (`drwxrwxrwx`) you can
skip this step. Otherwise, transfer ownership:

```bash
sudo chown -R 1003:1003 /opt/afc/databases
```

---

## 4. Configure `.env`

The `.env` file in the project root is the primary configuration file.  Copy it
and edit the mandatory fields listed below **before** starting the stack.  Every
field left at its shipped default value is either a security risk or will prevent
the stack from starting correctly.

> **Note:** The root `.env` is a shared template with no real secrets — it is
> tracked in git.  Each overlay's `.env` file (e.g. `myenv/.env`) holds
> deployment-specific paths and settings; it is excluded from git via
> `.gitignore`.  All actual credentials live in the overlay's `secrets/`
> directory (see [Step 6](#6-prepare-secrets-files)), which is also git-ignored.

### 4.1 Mandatory fields to change

#### Database root path

```ini
# Host directory containing ALL static databases (terrain, ULS, ITU, …)
VOL_H_DB=/opt/afc/databases/rat_transfer
```

#### Secrets directory

For standalone deployments set an absolute path:

```ini
# Host directory where per-service secret files live (see Step 6)
VOL_H_SECRETS=/opt/afc/secrets
```

For subfolder deployments (see [4.3](#43-working-in-a-deployment-subfolder)) use
a path relative to the project root, e.g. `./myenv/secrets`.

#### External IP binding

Uncomment and set to bind the web port on a specific interface, or leave
`0.0.0.0` to bind on all interfaces:

```ini
EXT_IP=0.0.0.0
```

#### Credentials

All passwords, tokens, and API keys are delivered as Docker secret files
mounted at `/run/secrets/` inside each container. Run the script in
[Step 6](#6-prepare-secrets-files) to generate all secret files before
starting the stack.

For the full list of credentials, how to generate them, and how to rotate them
after deployment, see [`SECURITY.md`](SECURITY.md).

### 4.2 Optional profiles

```ini
# Require mTLS client certificate on every AP-AFC request
AFC_ENFORCE_MTLS=true
```

To activate optional service groups, set `COMPOSE_PROFILES` in `.env` or on
the command line.  Available groups:

| Profile | What it adds |
|---|---|
| `monitor` | Grafana, Prometheus, cAdvisor, Loki, Alloy, Kafka UI |
| `msghnd` | Legacy message-handler request server (only when `AFC_REQ_SERVER=msghnd`) |

> **Monitoring adds resource overhead and expands the service surface.**
> Enable it only when actively monitoring the stack.  Review the dashboards
> and restrict network access to monitoring ports before exposing the host.
> See [Section 7.2](#72-with-monitoring-dashboards-grafana-prometheus-kafka-ui)
> for the start command and configuration details.

### 4.3 Working in a deployment subfolder

If you are running multiple independent deployments on the same host (e.g. a
`staging/` environment alongside a `release/` one), keep each deployment's secrets
isolated in its own subfolder rather than modifying the project-root `.env`.

```bash
# Create a named subfolder for this deployment (replace "myenv" with your chosen name)
ENVNAME=myenv
mkdir "$ENVNAME"
cd "$ENVNAME"

# Copy the root .env template and set per-deployment values
cp ../.env .
python3 - << EOF
import re

name = "$ENVNAME"
env_path = ".env"
with open(env_path) as f:
    text = f.read()

# Set the Compose project name so containers are named <name>-* and each
# overlay is fully isolated from others running on the same host.
text = re.sub(r'^(COMPOSE_PROJECT_NAME=).*\$', lambda m: m.group(1) + name, text, flags=re.MULTILINE)
# Point secrets directory to this deployment's subfolder (relative to project root)
text = re.sub(r'^(VOL_H_SECRETS=).*\$',  lambda m: m.group(1) + f'./{name}/secrets', text, flags=re.MULTILINE)
# RCACHE_RMQ_DSN placeholder — real password comes from the RCACHE_RMQ_PWD secret file
text = re.sub(r'^(RCACHE_RMQ_DSN=).*\$', lambda m: m.group(1) + 'amqp://rcache:x@rmq:5672/rcache', text, flags=re.MULTILINE)

with open(env_path, "w") as f:
    f.write(text)
print(f"Done. Review {name}/.env before starting the stack.")
EOF
```

All passwords and tokens are stored as secret files (see [Step 6](#6-prepare-secrets-files));
the `.env` fields for them are intentionally left empty.  See
[`SECURITY.md`](SECURITY.md) for the complete credential reference.

The project-root `.env` stays unchanged as a template. Each subfolder's `.env`
holds its own independent configuration.

> **Using multiple deployments in the same browser**: All deployments share the
> same session cookie name (`session`) and CSRF cookie name (`csrf_token`). If
> you access multiple deployments (e.g. local on port 5443 and prod on port 443)
> from the **same browser profile**, their session cookies will collide and you
> will be unexpectedly logged out or get authentication errors. To avoid this,
> use a **separate browser** (e.g. Firefox vs Chrome) or an **incognito/private
> window** for each deployment.

When starting the stack from the project root, pass `--env-file` and the
overlay compose file explicitly:

```bash
EXT_IP=0.0.0.0 TAG=<tag> docker compose \
    --env-file myenv/.env \
    -f docker-compose.yaml \
    -f myenv/docker-compose.yaml up -d --pull never
```

> **Private registry**: If your Docker images are not on `ghcr.io/open-afc-project`
> (the public default), add `PUB_REPO` and `PRIV_REPO` to the overlay `.env`:
> ```
> PUB_REPO=your.registry.example.com
> PRIV_REPO=your.registry.example.com
> ```
> `PUB_REPO` controls open-source images (ratdb, rmq, grafana, …);
> `PRIV_REPO` controls proprietary images (webui, afcserver, afc-worker, msghnd).

If you prefer to work at the project root with a single `.env`, adjust
`VOL_H_SECRETS` to an absolute path (e.g. `/opt/afc/secrets`) and use:

```bash
EXT_IP=0.0.0.0 TAG=<tag> docker compose \
    -f docker-compose.yaml up -d --pull never
```

---

## 5. Generate TLS Certificates

OpenAFC's dispatcher (nginx) terminates HTTPS and optionally enforces mTLS.
`scripts/gen_prod_certs.sh` creates a self-signed test CA, a server certificate,
and an AP client certificate — all valid for 10 years. For a real production
deployment, replace these with certificates from your organisation's CA.

The script accepts an optional output directory argument (default: current
working directory). All certificates are written under
`<outdir>/certs/clients/` and `<outdir>/certs/server/`.

```bash
# Run from the project root, output into a named subfolder:
./scripts/gen_prod_certs.sh myenv

# Or run from inside the overlay directory, output into ./certs/:
cd myenv
../scripts/gen_prod_certs.sh
```

This produces:

| Path (relative to outdir) | Purpose |
|---|---|
| `certs/clients/ca_crt.pem` | CA certificate — loaded by nginx as the mTLS client CA bundle |
| `certs/clients/client.bundle.pem` | Same CA cert as a bundle (copy) |
| `certs/clients/ap_client_crt.pem` | AP client cert — used by the test runner |
| `certs/clients/ap_client_key.pem` | AP client private key |
| `certs/server/server.cert.pem` | nginx TLS server certificate |
| `certs/server/server.key.pem` | nginx TLS server private key |
| `certs/server/server.bundle.pem` | cert + CA chain for nginx |

Your overlay `docker-compose.yaml` should mount these directories:

```yaml
services:
  dispatcher:
    volumes: !override
      - ./myenv/certs/server:/certificates/servers:ro
      - ./myenv/certs/clients:/etc/nginx/certs:ro
```

**When to re-run:** Certificates have 10-year validity. Re-run only if you
need to change the CN/SAN or are rotating credentials.

### 5.1 Production PKI: per-AP certificates

> **PKI** (Public Key Infrastructure) is the system of Certificate Authorities,
> key pairs, certificates, and revocation mechanisms that lets devices
> cryptographically prove their identity.

`gen_prod_certs.sh` issues **one shared client certificate** for the whole AP
fleet. This is acceptable for testing but not for production: if one device's
private key is exposed, the entire fleet's mTLS credential must be rotated
simultaneously.

For production, each AP must receive its **own certificate** so that individual
device credentials can be revoked without affecting the rest of the fleet.

#### What goes into each AP certificate

| Field | Recommended value | Why |
|---|---|---|
| `CN` (Common Name) | AP serial number, e.g. `AP-SN-00042` | Ties the TLS identity to the device; appears in ALS audit logs |
| `SAN` (Subject Alternative Name) | `DNS:<serial-number>` (optional) | Some TLS stacks require a SAN for extended key usage |
| `extendedKeyUsage` | `clientAuth` | Marks the cert for client authentication only |
| Validity | Aligned to device lifecycle (3–10 years) | Avoids mass re-issuance during normal operation |

#### Issuing a single per-AP certificate

After running `gen_prod_certs.sh` once to create the CA, you can issue
additional certificates without regenerating the CA:

```bash
# Variables
SERIAL="AP-SN-00042"          # AP serial number — becomes the cert CN
CA_DIR="myenv/certs/ca"
CLIENT_DIR="myenv/certs/clients"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# 1. Generate a unique key for this AP
openssl genrsa -out "$WORK/${SERIAL}.key.pem" 4096 2>/dev/null

# 2. Create a certificate signing request with the serial number as CN
openssl req -new \
    -key "$WORK/${SERIAL}.key.pem" \
    -out "$WORK/${SERIAL}.csr" \
    -subj "/C=US/O=YourOrg/CN=${SERIAL}" \
    -sha256

# 3. Write the extension file
cat > "$WORK/ap_ext.cnf" <<'EOF'
authorityKeyIdentifier=keyid,issuer
basicConstraints = CA:FALSE
extendedKeyUsage = clientAuth
keyUsage = critical, digitalSignature, keyEncipherment
subjectKeyIdentifier = hash
EOF

# 4. Sign with the deployment CA
openssl x509 -req \
    -in  "$WORK/${SERIAL}.csr" \
    -CA  "$CLIENT_DIR/ca_crt.pem" \
    -CAkey "$CA_DIR/ca_key.pem" \
    -CAcreateserial \
    -out "$WORK/${SERIAL}.crt.pem" \
    -days 3650 -extfile "$WORK/ap_ext.cnf" -sha256

# 5. Package as PKCS#12 for delivery to the AP
openssl pkcs12 -export \
    -in  "$WORK/${SERIAL}.crt.pem" \
    -inkey "$WORK/${SERIAL}.key.pem" \
    -out "$WORK/${SERIAL}.p12" \
    -name "$SERIAL" \
    -passout pass:""   # set a strong passphrase for real deployments

echo "Certificate and key bundle: $WORK/${SERIAL}.p12"
```

Deliver `${SERIAL}.p12` (or the `.crt.pem` + `.key.pem` pair) to the AP via
your provisioning pipeline (see mass-issuance guidance below).

#### Mass certificate issuance for large AP fleets

For hundreds or thousands of APs, wrap the single-cert steps above in a loop
over a serial-number list:

```bash
#!/usr/bin/env bash
# issue_ap_certs.sh — issue one cert per line of serials.txt
# Usage: ./issue_ap_certs.sh serials.txt myenv/
set -euo pipefail

SERIALS_FILE="$1"
OVERLAY="$2"
CA_DIR="$OVERLAY/certs/ca"
CLIENT_DIR="$OVERLAY/certs/clients"
OUT_DIR="$OVERLAY/certs/ap_fleet"
mkdir -p "$OUT_DIR"

while IFS= read -r SERIAL; do
    [[ -z "$SERIAL" || "$SERIAL" == \#* ]] && continue
    WORK=$(mktemp -d)
    trap 'rm -rf "$WORK"' EXIT

    openssl genrsa -out "$WORK/key.pem" 4096 2>/dev/null
    openssl req -new \
        -key "$WORK/key.pem" \
        -out "$WORK/csr.pem" \
        -subj "/C=US/O=YourOrg/CN=${SERIAL}" -sha256
    cat > "$WORK/ext.cnf" <<'EOF'
authorityKeyIdentifier=keyid,issuer
basicConstraints = CA:FALSE
extendedKeyUsage = clientAuth
keyUsage = critical, digitalSignature, keyEncipherment
subjectKeyIdentifier = hash
EOF
    openssl x509 -req \
        -in "$WORK/csr.pem" \
        -CA "$CLIENT_DIR/ca_crt.pem" \
        -CAkey "$CA_DIR/ca_key.pem" \
        -CAcreateserial \
        -out "$WORK/crt.pem" \
        -days 3650 -extfile "$WORK/ext.cnf" -sha256

    openssl pkcs12 -export \
        -in "$WORK/crt.pem" -inkey "$WORK/key.pem" \
        -out "$OUT_DIR/${SERIAL}.p12" \
        -name "$SERIAL" -passout pass:""

    echo "Issued: $SERIAL"
done < "$SERIALS_FILE"

echo "Done. PKCS#12 bundles are in $OUT_DIR/"
echo "Deliver each <serial>.p12 to the matching AP via your provisioning pipeline."
```

`serials.txt` is one AP serial number per line (blank lines and `#` comments
are ignored). The output is one `<serial>.p12` per AP in `certs/ap_fleet/`.

> **CA key security.** The CA private key (`certs/ca/ca_key.pem`) must be
> kept offline in an HSM or secrets manager after issuance. It is never
> mounted into any running container. Treat its exposure as a full fleet
> re-key event.

#### Delivering certificates to APs

The PKCS#12 bundle (`.p12`) contains the AP's certificate and private key.
Delivery methods depend on your manufacturing and field-provisioning process:

| Method | When to use |
|---|---|
| **Manufacturing-time injection** | Burn the PKCS#12 into the AP's secure storage (TPM, eFuse, or encrypted flash partition) during production. This is the most secure option — the private key never leaves a controlled environment. |
| **Zero-touch provisioning (ZTP)** | On first boot, the AP authenticates to a provisioning server using a device-unique bootstrap credential (e.g. factory-installed manufacturer cert) and retrieves its deployment certificate via EST (RFC 7030) or a proprietary enrollment protocol. |
| **Firmware image bundling** | Include the PKCS#12 in a signed, encrypted firmware image delivered via OTA update. Only appropriate when the firmware image itself is device-specific or the device storage is hardware-protected. |
| **Secure out-of-band delivery** | For small fleets or lab environments, transfer the `.p12` over SSH to an AP that already has a management interface. |

After delivery, the AP configures its TLS stack to present the certificate on
outbound connections to the AFC dispatcher (typically via an `ssl_certificate`
/ `ssl_certificate_key` configuration stanza or equivalent platform API).

#### The CA certificate does not go on APs

Only the **CA certificate** (`ca_crt.pem`) needs to be on the AFC server side
(mounted into the dispatcher). APs hold only their own certificate and private
key — they do not need the CA certificate to present their identity.

---

## 6. Prepare Secrets Files

AFC services read passwords and API keys from files mounted at `/run/secrets`
inside each container (Docker secrets mechanism). You must create one file per
secret on the host before starting the stack.

### Quick start: use `gen_secrets.sh`

```bash
# Create all missing secret files in one command (idempotent — safe to re-run):
./scripts/gen_secrets.sh myenv/        # e.g. ./scripts/gen_secrets.sh myenv/

# Rotate the AFC internal token on each re-deployment (recommended):
./scripts/gen_secrets.sh --rotate-token myenv/

# Rotate ALL random secrets (token + broker passwords + API keys):
./scripts/gen_secrets.sh --rotate-all myenv/

# or run from inside the overlay directory:
cd myenv
../scripts/gen_secrets.sh [--rotate-token|--rotate-all]
```

The script generates 256-bit random values for every secret that is missing
and prints what it created.  Secrets that require external credentials
(`CAPTCHA_SECRET`, `CAPTCHA_SITEKEY`, `GOOGLE_APIKEY`, `OIDC_CLIENT_ID`,
`OIDC_CLIENT_SECRET`, `MAIL_PASSWORD`) are created as **empty placeholder
files**.  The corresponding features (CAPTCHA, Google Maps, OIDC, email) are
disabled when their secret file is empty; fill in the real values when you need
them.  Docker Compose requires the files to exist before it can start the
stack, even for optional features — creating them automatically avoids the
`bind source path does not exist` startup error.

After running `--rotate-token`, restart the services that use the token:
```bash
docker compose ... up -d --pull never --no-deps afcserver rat_server worker msghnd
```

---

## 7. Start the Stack

### 7.1 Core stack

```bash
EXT_IP=0.0.0.0 TAG=<tag> \
  docker compose \
      --env-file myenv/.env \
      -f docker-compose.yaml \
      -f myenv/docker-compose.yaml up -d --pull never
```

Wait for `rat_server` to become healthy:

```bash
docker compose --env-file myenv/.env \
    -f docker-compose.yaml \
    -f myenv/docker-compose.yaml ps
```

### 7.2 With monitoring dashboards (Grafana, Prometheus, Kafka UI)

> **Monitoring is optional.** It adds Grafana, Prometheus, cAdvisor, Loki,
> Alloy, and Kafka UI — all of which consume additional CPU/memory and listen
> on extra host ports.  Enable only when you are actively observing the stack,
> and restrict network access to those ports on the host firewall.

Set `COMPOSE_PROFILES=monitor` and `AFC_GRAFANA_ENABLED=true`:

```bash
COMPOSE_PROFILES=monitor AFC_GRAFANA_ENABLED=true \
EXT_IP=0.0.0.0 TAG=<tag> \
  docker compose \
      --env-file myenv/.env \
      -f docker-compose.yaml \
      -f myenv/docker-compose.yaml up -d --pull never
```

If also using the legacy `msghnd` request server (`AFC_REQ_SERVER=msghnd`), add that profile:

```bash
COMPOSE_PROFILES=monitor,msghnd AFC_GRAFANA_ENABLED=true \
EXT_IP=0.0.0.0 TAG=<tag> \
  docker compose \
      --env-file myenv/.env \
      -f docker-compose.yaml \
      -f myenv/docker-compose.yaml up -d --pull never
```

> **`docker.sock` access**: The monitoring stack uses a
> `socket-proxy` service that exposes only read-only container and event
> endpoints over an internal network.  Direct host-socket mounts are not used.
> The `cadvisor` service mounts `/var/lib/docker` read-only (`:ro`) to read
> overlay2 container metadata; all unnecessary Linux capabilities are dropped
> (`cap_drop: ALL`) and `no-new-privileges: true` is set.

> **Grafana access**: Grafana uses nginx `auth.proxy` — any AFC user with the
> **Super** role is automatically signed in as a Viewer when they open
> `/fbrat/grafana/`.  No separate Grafana password is required for normal use.
> The `admin` account (password in `myenv/secrets/GRAFANA_ADMIN_PASSWORD`)
> exists for direct Grafana administration (e.g. adding datasources, managing
> users).
>
> The `auth.proxy` header is accepted only from the dispatcher container.
> Each overlay runs Grafana and the dispatcher on a dedicated internal Docker
> network (`grafana-proxy`) whose subnet must be unique across all stacks
> running on the same host to avoid Docker subnet conflicts.  Set three
> variables in your overlay's `.env` to a non-overlapping `/29` block:
>
> | Overlay | Variable | Example value |
> |---|---|---|
> | `local/` | `GRAFANA_PROXY_SUBNET` | `192.168.128.0/29` |
> | `local/` | `GRAFANA_PROXY_DISPATCHER_IP` | `192.168.128.2` |
> | `local/` | `GRAFANA_PROXY_GRAFANA_IP` | `192.168.128.3` |
> | `prod/` (prod-like) | `GRAFANA_PROXY_SUBNET` | `192.168.129.0/29` |
> | `prod/` (prod-like) | `GRAFANA_PROXY_DISPATCHER_IP` | `192.168.129.2` |
> | `prod/` (prod-like) | `GRAFANA_PROXY_GRAFANA_IP` | `192.168.129.3` |
>
> If two stacks are started with the same subnet, Docker will refuse to
> create the second network with an "overlaps with other one on this address
> space" error.  Assign each additional overlay a distinct `/29` block.
>
> | Access path | Who | How |
> |---|---|---|
> | `/fbrat/grafana/` via the AFC web UI | AFC Super users | Auto-login via nginx `auth.proxy` (Viewer role) |
> | Direct Grafana login | Grafana `admin` only | `admin` / `GRAFANA_ADMIN_PASSWORD` secret |
>
> Grafana creates its own PostgreSQL database via the AFC `CreateDb` API on
> first startup.  If the Grafana container started before `rat_server` was
> healthy (e.g. on a fresh deployment), the database creation fails silently
> and Grafana shows a blank page.  Fix by recreating the container after the
> stack is fully up:
> ```bash
> docker compose --env-file myenv/.env -f docker-compose.yaml \
>     -f myenv/docker-compose.yaml up -d --force-recreate --pull never grafana
> ```
>
> **Grafana 12.x navigation**: After opening `/fbrat/grafana/` as a Super user
> (auto-login via auth.proxy), the left sidebar shows **Dashboards**, **Explore**
> (for ad-hoc metric and log queries), and **Alerting**.  The **Drilldown**
> section (Logs Drilldown, Metrics Drilldown) appears for users with the
> Viewer role when `viewers_can_edit = true` is set in `grafana/templates/custom.ini`.
> Data panels will be empty on a fresh deployment until traffic flows and
> Alloy/Loki/Prometheus collect data.
>
> The RabbitMQ management UI is intentionally restricted to limit
> broker administration access.  Use `docker exec` to inspect RabbitMQ state
> instead:
> ```bash
> docker exec myenv-rmq-1 rabbitmqctl list_queues
> docker exec myenv-rmq-1 rabbitmqctl list_connections
> ```

### 7.3 mTLS enforcement

If your overlay `docker-compose.yaml` sets `AFC_ENFORCE_MTLS=true` on the
`dispatcher` service, every AP-AFC request must present a client certificate
signed by the overlay's test CA (generated in [Step 5](#5-generate-tls-certificates)).

Pass the client cert arguments to any test command:

```bash
cd tests
python3 afc_tests.py --addr localhost --port 443 --prot https \
  --cmd run \
  --ca_cert  ../myenv/certs/clients/ca_crt.pem \
  --cli_cert ../myenv/certs/clients/ap_client_crt.pem \
  --cli_key  ../myenv/certs/clients/ap_client_key.pem
```

### 7.4 Stop the stack

```bash
docker compose \
    --env-file myenv/.env \
    -f docker-compose.yaml \
    -f myenv/docker-compose.yaml down
```

---

## 8. First-time Initialisation

Run these commands once after the stack first starts.

### 8.1 Create the user database schema

```bash
docker compose --env-file myenv/.env \
    -f docker-compose.yaml -f myenv/docker-compose.yaml \
    exec rat_server rat-manage-api db-create --if_absent
```

### 8.2 Create the Super Administrator account

```bash
docker compose --env-file myenv/.env \
    -f docker-compose.yaml -f myenv/docker-compose.yaml \
    exec rat_server \
    sh -c 'echo "YourStrongPasswordHere" | \
        rat-manage-api user create \
            --role Super --role Admin --role AP --role Analysis \
            --password-file /dev/stdin \
            admin'
```

Or interactively (the command will prompt for the password):

```bash
docker compose --env-file myenv/.env \
    -f docker-compose.yaml -f myenv/docker-compose.yaml \
    exec -it rat_server \
    rat-manage-api user create \
        --role Super --role Admin --role AP --role Analysis \
        admin
```

You can now log into the WebUI at `https://<host>:443` with this account.

### 8.3 Import AFC configuration

The test configuration lives in the project source tree; it must be copied into
the `rat_server` container before importing.

```bash
# Copy the config file into the running container (note: underscore in rat_server)
docker cp tests/regression/pipe/export_admin_cfg.json myenv-rat_server-1:/tmp/

# Import it
docker compose --env-file myenv/.env \
    -f docker-compose.yaml -f myenv/docker-compose.yaml \
    exec rat_server \
    rat-manage-api cfg add src=/tmp/export_admin_cfg.json
```

This loads the AP configurations used by the 193 regression tests.

---

## 9. Validate the Deployment

After the stack is up and configured, run the 193-test regression suite to
confirm the deployment is correct.

### 9.1 Option A — Quick validation (~45 seconds, cache-served)

This option is fast because responses are served from the rcache after the first
unique request. It verifies end-to-end connectivity and correct response format.

```bash
cd tests
python3 afc_tests.py \
    --cmd run --addr localhost --port 443 --prot https \
    --outfile /tmp/afc_results.csv
```

All 193 tests should report `status Ok`.

### 9.2 Option B — Full engine validation (~30 minutes, forces engine computation)

This option confirms that the AFC engine and all static databases are correctly
mounted and produce the expected results. After the cache is cleared the
rcache background precomputer recomputes all results via the engine in parallel,
and the test requests share that work, so total runtime is roughly 1–2 minutes.

```bash
# Step 1: Clear the response cache — the rcache precomputer will then re-run
# every request through the engine so the test sees fresh computation results.
RCACHE_API_KEY=$(cat myenv/secrets/RCACHE_API_KEY) ./scripts/clear_rcache.sh

# Step 2: Run all 193 tests
cd tests
python3 afc_tests.py \
    --cmd run --addr localhost --port 443 --prot https \
    --outfile /tmp/afc_results_full.csv
```

All 193 tests should report `status Ok`.  The first run takes 1–2 minutes
(parallel engine computation via the rcache precomputer).  A second run without
clearing the cache completes in ~45 seconds (cache-served).

### 9.3 WebUI mode validation

The WebUI tests verify the full browser-facing request path via `rat_server`.
A session cookie from an authenticated Super-role user is required.

**Get the session cookie from the browser:**

1. Log in to the WebUI as a Super-role user.
2. Open browser DevTools → **Application** → **Cookies** → select the site.
3. Copy the value of the `session` cookie.

```bash
SESSION=<paste session cookie value here>
cd tests
python3 afc_tests.py \
    --cmd run --addr localhost --port 443 --prot https \
    --webui --session_cookie "$SESSION" \
    --outfile /tmp/afc_webui_results.csv
```

### 9.4 Production mTLS validation

To validate an overlay with mTLS enforced (see [Section 7.3](#73-mtls-enforcement)):

```bash
cd tests
python3 afc_tests.py \
    --cmd run --addr localhost --port 443 --prot https \
    --ca_cert  ../myenv/certs/clients/ca_crt.pem \
    --cli_cert ../myenv/certs/clients/ap_client_crt.pem \
    --cli_key  ../myenv/certs/clients/ap_client_key.pem \
    --outfile /tmp/afc_mtls_results.csv
```

---

## 10. Performance Testing

The `afc_load_tool.py` script sends AFC requests in parallel streams to measure
throughput. It can pre-populate the response cache with fake results for a
network-layer throughput test, or bypass the cache for an engine-computation
throughput test.

See [`tools/load_tool/README.md`](tools/load_tool/README.md) for full usage,
prerequisites, and configuration examples.

Quick start (cache-hit throughput test against a running stack):

```bash
cd tools/load_tool
pip install pyyaml requests
python3 afc_load_tool.py load \
    --url https://localhost:443 \
    --idx_range 1:200 \
    --workers 8
```

---

## 11. Ongoing Operations

### ULS database updates

The ULS downloader container runs daily automatic updates of the FCC ULS
database. See [`uls/README.md`](uls/README.md) for configuration, manual
trigger, and monitoring.

#### ULS hash-manifest integrity verification

The downloader enforces SHA-256 integrity on every downloaded SQLite file.
Configure exactly one of these env vars in your overlay (e.g. `myenv/.env`):

| Variable | Value | Use case |
|---|---|---|
| `ULS_HASH_MANIFEST_URL` | HTTPS URL of a team-maintained manifest JSON | Production (manifest updated by the team on each ULS release) |
| `ULS_HASH_MANIFEST_FILE` | Absolute **container** path to a local manifest JSON | Dev / offline environments |

If neither is set, the downloader logs an error and refuses new downloads
(fail-closed behaviour).

**Option A — Single-machine bootstrap (dev / offline):**

Generate the manifest from inside the running `uls_downloader` container,
then point `ULS_HASH_MANIFEST_FILE` at it:

```bash
# Run inside the container (the script is part of the uls image):
docker exec <project>-uls_downloader-1 python3 /wd/generate_hash_manifest.py \
    --manifest /rat_transfer/ULS_Database/uls_hash_manifest.json \
    --dir     /rat_transfer/ULS_Database/

# Then add to myenv/.env:
ULS_HASH_MANIFEST_FILE=/rat_transfer/ULS_Database/uls_hash_manifest.json
```

> **Note**: The generated manifest covers the databases present at generation
> time.  When the downloader fetches a *new* SQLite file, its hash will not
> yet be in the manifest and the download will be rejected.  After a successful
> manual download, re-run `generate_hash_manifest.py` to extend the manifest.
> For automated daily updates, use Option B below.

**Option B — Two-machine automated workflow (recommended for production):**

Run `uls/update_uls_manifest.sh` on a **separate machine** (different host,
different credentials) that has read access to the ULS Database directory.
The script calls `generate_hash_manifest.py` and optionally SCPs the result
to the production host.  This is the model described in
[`uls/README.md §SHA-256 hash manifest`](uls/README.md#integrity_hash).

```bash
# On a separate trusted machine — bootstrap manifest from current databases:
uls/update_uls_manifest.sh \
    --db-dir  /nfs/rat_transfer/ULS_Database/ \
    --manifest /nfs/rat_transfer/uls-hash-manifest.json \
    --scp-dest afc@prod-server:/opt/afc/databases/rat_transfer/uls-hash-manifest.json \
    --prune

# On the production host — bind-mount the manifest into the container
# and set in myenv/.env (or docker-compose override):
#   ULS_HASH_MANIFEST_FILE=/rat_transfer/uls-hash-manifest.json

# Cron job on the separate machine (run every 4 hours — same cadence as
# the uls_downloader download interval — so new entries are in the manifest
# before the next uls_downloader validation cycle):
0 */4 * * *  /opt/afc/brcm-afc/uls/update_uls_manifest.sh \
    --db-dir  /nfs/rat_transfer/ULS_Database/ \
    --manifest /nfs/rat_transfer/uls-hash-manifest.json \
    --scp-dest afc@prod-server:/opt/afc/databases/rat_transfer/uls-hash-manifest.json \
    --prune >> /var/log/uls_manifest.log 2>&1
```

See [`uls/README.md`](uls/README.md#integrity_hash) for the full trust model,
HMAC signing options, and why the manifest must be on a separate machine to
constitute an independent trust anchor.

### Response cache invalidation

The rcache holds pre-computed AFC responses and is automatically invalidated
when AFC configuration changes. To manually flush all cached responses (for
example, before a full engine-validation test run):

```bash
export RCACHE_API_KEY=$(cat myenv/secrets/RCACHE_API_KEY)
./scripts/clear_rcache.sh
```

The script calls `POST /invalidate` on the rcache service. The next AFC request
for each device will trigger a fresh engine computation and repopulate the cache.

### PostgreSQL upgrades

PostgreSQL 14 reaches end-of-life on 9 November 2026. See
[`PostgreSQL-upgrade.md`](PostgreSQL-upgrade.md) for the automated
dump-and-restore upgrade procedure.

### Credential rotation

See [`SECURITY.md`](SECURITY.md) for a table of all credentials, how to
rotate each one, and which containers log CRITICAL warnings if a known default
is detected at startup.

### Scaling

For high-availability or multi-node deployments, see
[`helm/README.md`](helm/README.md) for the Kubernetes / Helm deployment guide.
