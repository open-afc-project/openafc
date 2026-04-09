This work is licensed under the OpenAFC Project License, a copy of which is
included with this software program.

# OpenAFC Security Reference

This document is a single-page security reference for operators deploying
OpenAFC. It covers every credential that must be changed before production,
what happens if defaults are left in place, mTLS configuration, and the
`AFC_ENABLE_TEST_CERTS` flag.

For the full step-by-step deployment procedure, see
[`DEPLOYMENT.md`](DEPLOYMENT.md).

---

## Table of Contents

1. [Credentials that MUST be changed](#1-credentials-that-must-be-changed)
2. [What happens if defaults are left in place](#2-what-happens-if-defaults-are-left-in-place)
3. [Rotating credentials after deployment](#3-rotating-credentials-after-deployment)
4. [TLS and mTLS](#4-tls-and-mtls)
5. [AFC_ENABLE_TEST_CERTS — production prohibition](#5-afc_enable_test_certs--production-prohibition)
6. [Reporting security issues](#6-reporting-security-issues)

---

## 1. Credentials that MUST be changed

The table below lists every secret that has a known-bad shipped default.
**All of these must be replaced with random values before any deployment
accessible from outside a development workstation.**

| Credential | Location | Shipped default | Generate with | Services that warn at startup |
|---|---|---|---|---|
| `AFC_INTERNAL_TOKEN` | `secrets/AFC_INTERNAL_TOKEN` | none — **startup fails if absent** | `python3 -c "import secrets; print(secrets.token_hex(32))"` | `afcserver` (refuses to start if unset) |
| `BROKER_PWD` | `secrets/BROKER_PWD` | `celery` | `python3 -c "import secrets; print(secrets.token_hex(24))"` | `rat_server`, `msghnd`, `worker` |
| `RCACHE_RMQ_PWD` | `secrets/RCACHE_RMQ_PWD` | `rcache` | `python3 -c "import secrets; print(secrets.token_hex(24))"` | none (log warning added if default detected) |
| `BULK_PG_PASSWORD` | `secrets/DB_CREATOR_PASSWORD_BULK_POSTGRES` | not set | `python3 -c "import secrets; print(secrets.token_hex(16))"` | none |
| `FLASK_SECRET_KEY` | `secrets/FLASK_SECRET_KEY` | empty file | `python3 -c "import secrets; print(secrets.token_hex(32))"` | `rat_server`, `msghnd` |
| `RATDB_PASSWORD` | `secrets/RATDB_PASSWORD` | empty file | `python3 -c "import secrets; print(secrets.token_hex(16))"` | `ratdb`, `rat_server` |
| `RCACHE_API_KEY` | `secrets/RCACHE_API_KEY` | empty file | `python3 -c "import secrets; print(secrets.token_hex(32))"` | `rcache` (returns HTTP 503 if unset) |
| `OBJST_API_KEY` | `secrets/OBJST_API_KEY` | empty file | `python3 -c "import secrets; print(secrets.token_hex(32))"` | `objst` |

All credentials are delivered to containers as Docker secret files mounted at
`/run/secrets/`.  Leave the corresponding `.env` fields empty; do not embed
passwords in environment variables.

Use `scripts/gen_secrets.sh <overlay_dir>` to generate all missing secret
files in one command (see [`DEPLOYMENT.md`](DEPLOYMENT.md) Step 6).

---

## 2. What happens if defaults are left in place

### `AFC_INTERNAL_TOKEN`

- `AFC_INTERNAL_TOKEN` has **no shipped default**. If the secret file is absent
  or empty when `afcserver` starts, startup is **refused with `SystemExit(1)`**
  and a CRITICAL log message is emitted. Write a random value to
  `secrets/AFC_INTERNAL_TOKEN` before starting the stack. Generate one with:
  ```
  python3 -c "import secrets; print(secrets.token_hex(32))"
  ```

### `BROKER_PWD`

- At startup, `rat_server`, `msghnd`, and `worker` log a **CRITICAL** warning
  if a placeholder default value is detected.
- Change `BROKER_PWD` to a strong random value before any production deployment.
  Write the new value to `secrets/BROKER_PWD`; do not set it as a plain
  environment variable.
- If the secret file is absent or empty, the service falls back to the
  built-in default and logs a CRITICAL error; it will start in development
  mode but must not be used in production.

### `RCACHE_API_KEY`

- If the secrets file is empty, the rcache `verify_token` function raises
  **HTTP 503 Service Unavailable** on every call (fail-closed behaviour). This
  prevents silent unauthenticated access at the cost of disabling cache
  writes/invalidation until the key is configured.

---

## 3. Rotating credentials after deployment

### Rotating `AFC_INTERNAL_TOKEN`

1. Generate a new token:
   ```bash
   NEW_TOKEN=$(python3 -c "import secrets; print(secrets.token_hex(32))")
   ```
2. Write it to the secret file:
   ```bash
   echo "$NEW_TOKEN" > /opt/afc/secrets/AFC_INTERNAL_TOKEN
   ```
3. Restart the affected services:
   ```bash
   docker compose restart afcserver rat_server msghnd worker
   ```
   There is a brief window during rolling restart where requests may fail; plan
   for a maintenance window if zero-downtime is required.

### Rotating `BROKER_PWD`

The RabbitMQ broker password is substituted into `definitions.json` at container
startup. To rotate:

1. Generate a new password:
   ```bash
   NEW_PWD=$(python3 -c "import secrets; print(secrets.token_hex(24))")
   ```
2. Write it to the secret file:
   ```bash
   echo "$NEW_PWD" > /opt/afc/secrets/BROKER_PWD
   ```
3. Restart the `rmq` container first (to pick up the new `definitions.json`),
   then restart the services that connect to it:
   ```bash
   docker compose restart rmq
   docker compose restart rat_server msghnd worker
   ```

### Rotating `RCACHE_RMQ_PWD`

1. Generate a new password:
   ```bash
   NEW_PWD=$(python3 -c "import secrets; print(secrets.token_hex(24))")
   ```
2. Write it to the secret file:
   ```bash
   echo "$NEW_PWD" > /opt/afc/secrets/RCACHE_RMQ_PWD
   ```
3. Restart `rmq`, then `rcache`, then the worker:
   ```bash
   docker compose restart rmq rcache worker
   ```

### Rotating file-based secrets (`FLASK_SECRET_KEY`, `RATDB_PASSWORD`, etc.)

1. Write the new value to the secrets file:
   ```bash
   python3 -c "import secrets; print(secrets.token_hex(32))" \
       > /opt/afc/secrets/FLASK_SECRET_KEY
   ```
2. Restart the container(s) that use the secret:
   ```bash
   docker compose restart rat_server msghnd
   ```

---

## 4. TLS and mTLS

### Server TLS

All HTTPS traffic is terminated at the nginx dispatcher. The server certificate
and key are mounted from the host. For production use, replace the self-signed
certificates produced by `scripts/gen_prod_certs.sh` with certificates issued
by a trusted CA.

Certificate files the dispatcher expects:

| File (inside container) | What it contains |
|---|---|
| `/certificates/servers/server.cert.pem` | Server TLS certificate |
| `/certificates/servers/server.key.pem` | Server TLS private key |
| `/certificates/servers/server.bundle.pem` | Cert + intermediate chain |

Both `docker-compose.local.yaml` and the optional local prod overlay mount
server certs from `./secrets/`.

### mTLS (AP client certificates)

AP devices must present a client certificate signed by a trusted CA when mTLS
is enforced. Control mTLS with the `AFC_ENFORCE_MTLS` environment variable:

| Value | Behaviour |
|---|---|
| `false` (default) | Client cert optional; requests without a cert are passed through |
| `true` | Client cert required; nginx returns HTTP 403 for requests without a valid cert |

Set `AFC_ENFORCE_MTLS=true` in your override compose file for production.

The CA bundle nginx uses to verify client certs is mounted from:
- `docker-compose.local.yaml`: `./dispatcher/certs/clients/` (self-signed dev CA; mTLS optional)
- prod overlay: `./dispatcher/certs/prod_clients/` (prod-CA-signed bundle from `gen_prod_certs.sh`; mTLS enforced)

#### How mTLS works

During every AP-AFC connection the following steps occur:

1. **TLS handshake** — The AP presents its client certificate. nginx verifies the
   certificate chain against the CA bundle mounted at
   `/etc/nginx/certs/client.bundle.pem`.

2. **Header forwarding** — When verification succeeds, nginx extracts the
   certificate Subject Distinguished Name (DN) and forwards two headers upstream:
   - `mTLS-DN: <subject DN>` — the verified identity of the connecting AP
   - `X-SSL-Client-Verify: SUCCESS` — nginx attestation that the cert was verified

   When verification fails (no cert, expired cert, unknown CA), nginx forwards
   `mTLS-DN` as an empty string and `X-SSL-Client-Verify` as `FAILED` or
   `NONE`. When `AFC_ENFORCE_MTLS=true`, nginx returns HTTP 403 immediately and
   the upstream AFC server is never reached.

3. **Dispatcher-token check** — The AFC server validates a shared
   `AFC_DISPATCHER_TOKEN` that is mounted only into the nginx dispatcher and
   the AFC server. This token proves the `mTLS-DN` and `X-SSL-Client-Verify`
   headers originated from nginx, not from an external caller.

4. **mTLS-DN enforcement** — When `AFC_ENFORCE_MTLS=true`, the AFC server
   rejects any request that arrives without a verified `mTLS-DN` header, even
   if nginx already passed it through.

5. **Audit logging** — The mTLS DN is recorded in ALS alongside every AFC
   request, providing a per-connection audit trail tied to the certificate
   identity.

#### Cert DN vs. AFC serial number

The mTLS client certificate and the `serialNumber` field in the AFC request
payload serve **different, complementary purposes**:

| | mTLS client certificate | AFC `serialNumber` |
|---|---|---|
| Protocol layer | Transport (TLS handshake) | Application (JSON payload) |
| What it proves | The connecting device holds a key signed by the deployment CA | The device type's FCC certification ID |
| Extracted from | Certificate Subject DN → `mTLS-DN` header | `deviceDescriptor.serialNumber` in the request body |
| Used for | Connection-level access control, audit logging | Spectrum authorisation lookup, per-device deny-list |

**The AFC server does not cross-check the cert CN against `serialNumber`.**
The two identities are independent: mTLS proves the device was provisioned by
your CA; the serial number drives application-layer certification and
deny-listing. Per-device deny-list enforcement therefore relies on the
`serialNumber` value in the request body being accurate.

For stronger per-device traceability, set `CN=<serial-number>` in each AP's
individual certificate (see [DEPLOYMENT.md §5](DEPLOYMENT.md#5-generate-tls-certificates)
for per-AP issuance guidance). This allows ALS audit logs to be correlated
with specific device serial numbers by inspecting the `mTLS-DN` field.
Server-side enforcement that the cert CN matches `serialNumber` is an optional
hardening step beyond the current implementation.

#### Certificate revocation (CRL)

`gen_prod_certs.sh` creates an initial empty CRL (`client.crl.pem`) signed by
the deployment CA. nginx loads this CRL at startup via `ssl_crl`. When a
certificate must be revoked:

1. Use `openssl ca -revoke` against the leaf cert with the CA key and
   database created by `gen_prod_certs.sh` (stored in a temporary directory
   during the run — you must maintain your own CA database for ongoing
   operations).
2. Re-generate the CRL:
   ```bash
   openssl ca -gencrl \
       -keyfile certs/ca/ca_key.pem \
       -cert   certs/clients/ca_crt.pem \
       -out    certs/clients/client.crl.pem \
       -config ca.cnf
   ```
3. Copy the updated `client.crl.pem` to every host running the dispatcher
   (`./dispatcher/certs/clients/` or your production cert mount).
4. Signal nginx to reload its configuration:
   ```bash
   docker compose exec dispatcher nginx -s reload
   ```

nginx will then reject any connection from an AP whose certificate serial
number appears in the CRL, without restarting the service.

> **Keep the CA private key offline.** After initial cert issuance, store
> `certs/ca/ca_key.pem` in a secrets manager or air-gapped system. It is
> never mounted into any container — only the CA certificate (`ca_crt.pem`)
> is needed by nginx at runtime.

#### Generating self-signed test certificates

```bash
./scripts/gen_prod_certs.sh
```

This creates a test CA, a server cert, and a shared AP client cert/key.
Server certs are written directly into `secrets/`; client CA files go under
`dispatcher/certs/prod_clients/`. See
[`DEPLOYMENT.md`](DEPLOYMENT.md) for the full output listing.

#### Managing mTLS certificates via CLI

```bash
# List loaded CA bundles
docker compose exec rat_server rat-manage-api mtls list

# Add a CA bundle
docker compose exec rat_server \
    rat-manage-api mtls create \
    --src /path/to/ca_bundle.pem \
    --org "My Organisation" \
    --note "Prod AP CA"

# Remove a bundle (use ID from list)
docker compose exec rat_server rat-manage-api mtls remove --id <ID>
```

---

## 5. AFC_ENABLE_TEST_CERTS — production prohibition

`AFC_ENABLE_TEST_CERTS` is a development-only variable that, when set to `true`,
registers the hard-coded identifiers `TestCertificationId` /
`TestSerialNumber` (and `HeatMapCertificationId` / `HeatMapSerialNumber`) as
valid certified devices — **without any database registration**.

**This variable MUST NOT be set to `true` in production.** When enabled:

- Device-certification enforcement is disabled for any device presenting
  these development test credentials. This is intended strictly for local
  development and automated CI testing; it must never be active in production.
- `afcserver` and `rat_server` log a **CRITICAL** warning at startup.

If you need trial or test users in production, register their AP configurations
through the Admin UI and assign them the `Trial` role. Do not use
`AFC_ENABLE_TEST_CERTS=true` outside of isolated development environments.

---

## 6. Reporting security issues

Security vulnerabilities in OpenAFC should be reported privately. Please do not
file a public GitHub issue for a security vulnerability.

Use the GitHub Security Advisories feature:
[https://github.com/open-afc-project/openafc/security/advisories/new](https://github.com/open-afc-project/openafc/security/advisories/new)

For questions about deployment security, use the
[Q&A Discussions](https://github.com/open-afc-project/openafc/discussions/categories/q-a).
