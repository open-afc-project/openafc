Copyright (C) 2022 Broadcom. All rights reserved.\
The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate that
owns the software below. This work is licensed under the OpenAFC Project
License, a copy of which is included with this software program.

# AFC System secrets and their handling

## Table of contents
- [Secrets](#secrets)
- [Kubernetes secrets manifest](#manifest)
- [Secrets template](#template)
- [Secrets handling in production (Kubernetes) environment](#kubernetes)
- [Secrets handling in development (Compose) environment](#compose)
- [`secret_tool.py` - secrets handling tool](#secret_tool)
  - [`check` subcommand (check manifest/template validity)](#check)
  - [`list` subcommand (list secrets)](#list)
  - [`export` subcommand (make Compose secrets)](#export)
  - [`compare_template` subcommand (verify template consistency)](#compare_template)
  - [Sample template](#sample_template)

## Secrets <a name="secrets"/>

**Secret** is a configuration data that should remain, well, secret fully or partially - such as passwords, certificates, API keys, etc.
Example of partially secret data is email configuration that includes secret username and password and, say, public SMTP server name.
If it is desired for such a data to be stored together - it become a secret, even though not all its parts are secret.

Secrets are secret (not stored in public repositories, protected at runtime, etc.), but secret nomenclature and inner structure (if secret contains more than one field) should remain public.

Secrets should be available to AFC applications in similar way both in development (**Compose**) and in production (**Kubernetes**) environments.
It is not expected from secrets in development environment to be secret from developers.
In production environment, however, secrets should be kept secret from everybody but applications that use them and The Supreme Administrators, endowed with The Great Round Seal.

Secrets are named entities. System configuration determines what secrets will be available to what container.
For AFC Applications running in containers secrets will be files (named per secret names, directory in Kubernetes may very, in Compose - always `/run/secrets`).

## Kubernetes secrets manifest <a name="manifest"/>
In Kubernetes the simplest (by no means safest) form of storing secrets is **secrets manifest file**. It is YAML file of the following general structure:
```
apiVersion: v1
kind: Secret
metadata:
    name: SECRETS_GROUP_NAME
data:
    # Comment1
    SECRET_NAME1: <Base64 secret value>
    # Comment2
    SECRET_NAME2: <Base64 secret value>
    ...
stringData:
    # Comment3
    SECRET_NAME3: <verbatim secret value>
    # Comment4
    SECRET_NAME4: <verbatim secret value>
    ...

    # Comment5
    JSON_SECRET1.json: |
        {
            "FIELD1": "VALUE1",
            ...
        }

    # Comment6
    YAML_SECRET1.yaml: |
        ---
        FIELD1: VALUE1
        ...

```
Note how YAML format allows to store JSON and YAML files in verbatim secrets section (`JSON_SECRET1.json`, `YAML_SECRET1.yam`).

This Kubernetes secret manifest format is used to store secrets (at least in Compose environment - Kubernetes storage mechanism may evolve towards something safer/more appropriate).

## Secrets template <a name="template"/>
Secrets values (passwords, certificates, keys, etc.) are secret, but secrets structure (set of secret names, inner structure of secrets) is public.

This structure is documented in secret template file. Secret template file is secret manifest in which all values are cleared. Like this:
```
apiVersion: v1
kind: Secret
metadata:
    name: SECRETS_GROUP_NAME
data:
    # Comment1
    SECRET_NAME1:
    # Comment2
    SECRET_NAME2:
    ...
stringData:
    # Comment3
    SECRET_NAME3:
    # Comment4
    SECRET_NAME4:
    ...

    # Comment5
    JSON_SECRET1.json: |
        {
            "FIELD1": null,
            ...
        }

    # Comment6
    YAML_SECRET1.yaml: |
        ---
        FIELD1:
        ...

```
Note that verbatim values for YAML/JSON-formatted secrets are not empty - instead fields in them are empty. This allows to maintain proper documenting of secrets' structure.

## Secrets handling in production (Kubernetes) environment <a name="kubernetes"/>
The specific mechanism of secrets' handling in production (Kubernetes) environment is out of scope of this document.
If secret manifest file will be used as is - fine. If something more secure and it will require some support - `secret_tool.py`, described later in this document will need to be upgraded to provide such a support.

Mechanism of distributing secrets to containers is also out of scope of this document (at least for now).

## Secrets handling in development (Compose) environment <a name="compose"/>

In Compose (development) environment secrets are stored in files (one file per secret). In the `docker-compose.yaml` file it looks like this:
```
version: '3.2'
services:
  service1:
    image: image1
    secrets:
      - SECRET_NAME1
      - SECRET_NAME2
  service2:
    image: image2
    secrets:
      - JSON_SECRET1.json:

secrets:
  SECRET_NAME1:
    file: <Filename for SECRET_NAME1>
  SECRET_NAME2:
    file: <Filename for SECRET_NAME2>
  JSON_SECRET1.json:
    file: <Filename for JSON_SECRET1.json>
```

Here `secrets` clause inside service definitions declares what services receive what secrets.
`services` top-level element declares in what file each secret is stored outside of container.

There is nothing safe about this scheme (albeit files may be protected with access rights), but no safety should be expected of development environment.

## `secret_tool.py` - secrets handling tool <a name="secret_tool"/>

**`secret_rool.py`** is a script for maintaining secret manifest files and templates.
It can work with many manifests/templates at once, albeit it is not quite clear yet if we'll have *so* many secrets that will justify use of several manifests.

General usage format is:
`secret_tool.py SUBCOMMAND [OPTIONS] MANIFEST_FILE(S)`

Options are subcommand-specific, will be reviewed together with subcommands.

### `check` subcommand (check manifest/template validity) <a name="check"/>
Checks validity of secret manifest/template files: valid YAML, values are strings (Nones in template), no duplicates, etc.

Options:

|Option|Meaning|
|------|-------|
|--template|File is a template, hence all secret values should be empty (for YAML/JSON verbatim secrets - scalar field values should be empty/null). By default - all secret values should be strings (valid base64 strings if in `data` section)|
|--local|If several manifests/templates specified - they allowed to have same secret name in different files (but not in same file)|

Examples:  
`secret_tool.py check afc_secrets.yaml`  
`secret_tool.py check --template templates/*.yaml`

### `list` subcommand (list secrets) <a name="list"/>
List secret names in given manifest/template files.

### `export` subcommand (make Compose secrets) <a name="export"/>
Export secrets from manifest file to directory (one file per secret), generate `secrets` section for `docker-compose.yaml`.

Options:

|Option|Meaning|
|------|-------|
|--secrets_dir **DIRECTORY**|Directory to write secrets to|
|--empty|Secrets written to directory specified by `--secrets_dir` will be empty (i.e. set of empty files will be written). Template(s) may be used instead of manifest(s) as source(s) for this case|
|--compose_file **COMPOSE_FILE**|Compose file to appends `secrets` block to (file created if not existing)|
|--base_dir **DIR_OR_MACRO**|Directory prefix to use in `secrets` block. If not specified directory from `--secrets_dir` is used if it is specified, '.' otherwise|

Example:  
Export secrets from manifest file `afc_secrets.yaml` to `/opt/afc/secrets` directory, while also append `secrets` block to `docker-compose.yaml`, using value of `SECRETS_DIR` variable (e.g. defined in `.env`) as directory prefix in `secrets` block:
```
secret_tool.py export --secrets_dir /opt/afc/secrets \
    --compose_file docker-compose.yaml --base_dir '${SECRETS_DIR} \
    afc_secrets.yaml
```

### `compare_template` subcommand (verify template consistency) <a name="compare_template"/>
Compare manifest(s) with correspondent template(s). Template files should have same names as correspondent manifest files.

Options:

|Option|Meaning|
|------|-------|
|--template_dir **DIRECTORY**|Directory containing template files|

Example:  
Compare content of `afc_secrets.yaml` manifest with `templates/afc_secrets.yaml` template:  
`secret_tool.py compare_template --template_dir templates afc_secrets.yaml`

## Sample template <a name="sample_template"/>
As of time of this writing the template for AFC Secrets looked like this:
```
# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.
---
apiVersion: v1
kind: Secret
metadata:
    name: afc_secrets
stringData:
    OIDC.json: |
        {
            "LOGIN": null,
            "CLIENT_ID": null,
            "CLIENT_SECRET": null,
            "DISCOVERY_URL": null
        }

    REGISTRATION_CAPTCHA.json: |
        {
            "USE_CAPTCHA": null,
            "SECRET": null,
            "SITEKEY": null,
            "VERIFY": null
        }

    NOTIFIER_MAIL.json: |
        {
            "SERVER": null,
            "PORT": null,
            "USE_TLS": null,
            "USE_SSL": null,
            "USERNAME": null,
            "PASSWORD": null
        }

    REGISTRATION.json: |
        {
            "DEST_EMAIL": null,
            "DEST_PDL_EMAIL": null,
            "SRC_EMAIL": null,
            "APPROVE_LINK": null,
        }
```
