Copyright (C) 2022 Broadcom. All rights reserved.  
The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate that
owns the software below. This work is licensed under the OpenAFC Project
License, a copy of which is included with this software program.

# Running OpenAFC server on Kubernetes

## Table of contents

- [Overview](#overview)
- [OpenAFC cluster structure](#cluster_structure)
  - [Kubernetes platform dependencies](#kubernetes_dependencies)
  - [Prerequisite components](#prerequisites)
  - [Clusters](#clusters)
  - [Pods](#pods)
  - [Image registries](#registries)
  - [Secrets](#secrets)
  - [Database DSNs and their passwords](#dsns_and_passwords")
- [Helmcharts](#helmcharts)
  - [File tree](#file_tree)
  - [Manifests](#manifests)
  - [`values.yaml` organization](#values_organization)
  - [`values.yaml` bulk overrides](#values_overrides)
    - [Feature-specific bulk overrides](#values_overrides_features)
    - [Site-specific, cluster-specific bulk overrides](#values_overrides_cluster_specific)
    - [Site-specific, common bulk overrides](#values_overrides_site_specific)
- [Scripts](#scripts)
  - [The **`AUTO`** name](#auto)
  - [`k3d_registry.py`](#k3d_registry_py)
  - [`k3d_cluster_create.py`](#k3d_cluster_create_py)
  - [`k3d_ports.py`](#k3d_ports_py)
  - [`push_images.py`](#push_images_py)
  - [`install_prerequisites.py`](#install_prerequisites_py)
  - [`helm_install_int.py`](#helm_install_int_py)
  - [`helm_install_ext.py`](#helm_install_ext_py)
- [Deployment](#deployment")
  - [`k3d` deployment](#k3d_deployment")
    - [Required software](#k3d_software)
    - [File preparations](#k3d_files")
      - [Static files](#k3d_staic_files")
      - [Secrets](#k3d_secrets)
      - [`values.yaml` override](#k3d_values_override")
    - [Create (start) cluster](#k3d_start")
    - [Exposed ports](#k3d_exposed_ports")
    - [Reload](#k3d_reload")
    - [Stop (delete) cluster](#k3d_stop")
  - [`GCP` deployment](#gcp_deployment")
     - [Provisioning](#gcp_provisoning")
     - [Config override files](#gcp_config_overrides)
     - [Putting images to registry](#gcp_push_images)
     - [Installing prerequisites](#gcp_install_prerequisites")
     - [Installing/reinstalling OpenAFC helmcharts](#cp_install_helmcharts")
     - [Uninstalling OpenAFC helmcharts](#gcp_uninstall_helmcharts")
     - [Uninstalling prerequisites](#gcp_uninstall_prerequisites")


## Overview <a name="overview">
`OpenAFC` runs on several container platforms:
1. **Kubernetes production.** Kubernetes on some cloud platform. This document covers `GCP`, but `AWS` is also a possibility.
2. **Kubernetes development.** Simplified Kubernetes platform (for playing with autoscaling, secrets and other Kubernetes-specific stuff). This document covers `k3d`, however `minikube` is also a possibility (`minikube` successfully tried on `Ubuntu`, but was abandoned because of `CentOS7` compatibility issues), other similar platforms were not tried.
3. **Development/testing.** Docker Compose - used for feature development.

This document only covers OpenAFC deployment on Kubernetes platform (#1 and #2).

One who wants to skip prolonged treatise and cut to the `kubectl` chase may go straight to [Deployment](#deployment) chapter.

## OpenAFC cluster structure <a name="cluster_structure">

### Kubernetes platform dependencies <a name="kubernetes_dependencies">

Like any Kubernetes application, OpenAFC requires certain resources of platform it runs on:

|Resource|`k3d`|`GCP`|
|--------|-----------|----------|
|Docker image registry|Platform's own locally-running registry|External registry - e.g. `JFrog Artifactory`|
|File storage|`hostPath` volumes|`NFS` volumes provided by cloud platform|
|SQL Databases|`PostgreSQL+PostGIS` pods (run inside cluster, not part of platform)|`PostgreSQL+PostGIS` resources of cloud platform|
|Secrets|`ExternalSecret` operator with `Fake` backend|`ExternalSecret` operator with `gcpsm` backend|


### Prerequisite components <a name="prerequisites">
Stuff (pods, services, daemonsets, APIservices, CRDs., etc.) running inside/alongside OpenAFC Kubernetes clusters to maintain proper environment:

|Component|Source|Role|
|---------|------|----|
|External Secret|`external-secrets` helmchart of <https://charts.external-secrets.io>|Delivers secrets to cluster from external secret store|
|Ingress-nginx|`ingress-nginx` helmchart of <https://kubernetes.github.io/ingress-nginx>|`Nginx`-based ingress, used on the internal `GCP` cluster|
|Prometheus|Manifests at `helm/monitoring/prometheus`|Hub for performance time series (metrics) collection|
|Prometheus operator|<https://github.com/prometheus-operator/prometheus-operator/releases/download/v0.72.0/bundle.yaml> manifest|Kubernetes operator allowing to describe custom `Prometheus` metrics|
|Prometheus adapter|`prometheus-adapter` helmchart of <https://prometheus-community.github.io/helm-charts>|Kubernetes ApiService allowing `Prometheus` metrics to be used by Kubernetes Horizontal Pod Autoscaling (`HPA`)|
|Other stuff|Security guys, cloud platform|`istio`, `calico` and other doodads security guys can't live without. OpenAFC does not directly or indirectly use them and does its best to counter impending performance degradation|


### Clusters <a name="clusters">

OpenAFC in Kubernetes `GCP` configuration uses two clusters: external and internal:
- **External cluster**. Runs *dispatcher* pod and service. *Dispatcher* pod runs `Nginx` that performs HTTPS/TLS/mTLS termination and passes HTTP traffic to internal cluster. Same container runs `Nginx` reloader script that reloads `Nginx` on mTLS certificates update.
- **Internal cluster** runs the essential part of OpenAFC. Has two external interfaces:
  - **ingress-nginx** exposing everything that can be exposed over it
  - **RabbitMQ** used to notify *dispatcher*'s `Nginx` reloader script about mTLS updates. RabbitMQ also has REST API interface, thus eventually it may migrate under `ingress-nginx`.

In `k3d` configuration everything runs in single cluster (and several such clusters may run on a single computer, which is useful for development).
Moreover, `k3d` cluster runs with one server and one agent, i.e. all pods run on a single 'node' (this does not preclude playing with autoscaling - but created pods do not cause node creation).

Besides 'official' OpenAFC entrypoint of *dispatcher*, OpenAFC on `k3d` exports various ports of pods running in cluster - use `helm/bin/k3d_ports.py <CLUSTERNAME>` to print them.


### Pods <a name="pods">
Each OpenAFC image runs in its own pod (i.e. no image affinity is assumed). Some images run in several pods as a result of autoscaling. On `GCP` platform pods have numerous sidecar images for greater security good, but presence of these sidecars is not assumed by OpenAFC images.

Here is a list of OpenAFC images (named by their deployments/statefulsets, to continue Docker Compose naming tradition):

|Deployment/<br>statefulset|Scaling|Role|
|--------------------------|------|-----|
|dispatcher|1|Entrypoint `Nginx`. Terminates HTTPS, mTLS. Dispatches AFC requests to *afcserver*, other requests - to *rat-server*|
|afcserver|1 to few|Processes normal AFC requests: either satisfies them from `Rcache` (Response Cache) database, or dispatches them to *worker* for `AFC Engine` computation. Can serve up to ~400-600 cached requests per second per `FastAPI` worker and not IO-bound - may be autoscaled based on CPU load|
|msghnd (deprecated)|1|Old `Flask`-based version of *afcserver*. Phased out|
|rat-server|0, was 1|Processes requests from Web UI (including but not limited to AFC) and requests from other pods|
|worker|1 to many|`AFC Engine` performs AFC computation. May be autoscaled based on number of outstanding work items in RabbitMQ Celery queue|
|uls-downloader|1|Downloads FS databases (ULS, etc.)|
|cert-db|1|Notifies *dispatcher* about changes in mTLS certificates via *rmq*|
|rmq|1|RabbitMQ. Transports Celery work items from *afcserver*/*rat-server* to worker, results in reverse direction, TLS Certificate update notifications from *cert-db* to *dispatcher*, etc. Eats surprising amount of CPU, even when idle.|
|rcache|1|Updates/invalidates AFC Response cache database|
|objst|1|File storage aspiring to be an object storage. Stores mTLS certificates, files resulting from, Web UI requests, etc.|
|als-kafka|1|Intermediate storage of ALS and ALS JSON log records en route to *als-siphon*|
|als-siphon|1|Stores incoming ALS and ALS JSON log records to ALS databases|
|ratdb|0 or 1|`PostgreSQL` database server for most valuable data (users, AP certifications and blacklists, etc.) Only used on `k3d` platform, as `GCP` uses cloud database resources|
|bulk-postgres|0 or 1|`PostgreSQL+PostGIS` database for various generated data: ALS, ALS Log, Rcache, ULS downloader state, etc.|
|grafana|1|As of time of this writing it just an empty shell, not filled with dashboards and users|
|nginxexporter|1|Export basic `Nginx` metrics to `Prometheus`|


### Image registries <a name="registries">

Historically there was an intention to keep OpenAFC images in two separate registries - public (*ratdb*, *rmq*, *dispatcher*, *objst*, *als-kafka*, *als-siphon*, *bulk-postgres*, *uls-downloader*, *cert-db*, *rcache*, *grafana*, *nginxexporter*) and private (*rat-server*, *msghnd* (deprecated), *afcserver*, *worker*).

Currently this idea is abandoned and all images are stored in the same registry. Images are still marked as private/public registries - maybe tis will be eliminated in future.


### Secrets <a name="secrets">

Secrets used by pods are stored in some external secret store and retrieved from there to cluster(s) by `ExternalSecret` operator (one of OpenAFC prerequisite components). `ExternalSecret` does not store secrets, rather it transfers them from some backend secret store (`gcpsm` on `GCP` platform, `fake` on `k3d` platform).

On `GCP` platform secret that `ExternalSecret` uses as a credential ('password') to `gcpsm` should be placed to cluster secrets during cluster provisioning, all other secrets may be stored in `gcpsm`.

Secrets used by pods are always have a form of files, i.e. secrets in environment variables are not used. Rather there are environment variables with names of files with secrets (unless these filenames are predefined - like `Nginx` certificate file names).

Here are list of secrets used by OpenAFC:

|Name|Pods|Role|
|----|----|----|
|image-pull|None|Image pull secret (not needed on `k3d`). Has `kubernetes.io/dockerconfigjson` type, see <https://kubernetes.io/docs/concepts/configuration/secret/#docker-config-secrets> or <https://kubernetes.io/docs/tasks/configure-pod-container/pull-image-private-registry/#registry-secret-existing-credentials> on how to prepare it|
|postgres-password|als-siphon|Password for PostgreSQL DSN with user `postgres` - used to create databases, if they are absent|
|ratdb-password|rat-server, msghnd (deprecated), afcserver, objst, cert-db|Password for PostgreSQL DSN for RatDB database (may be redirected to *postgres-password* - see [Database DSNs and their passwords](#dsns_and_passwords))|
|uls-state-db-password|uls-downloader|Password for PostgreSQL DSN for database with ULS Downloader state (may be redirected to *postgres-password* - see [Database DSNs and their passwords](#dsns_and_passwords))|
|als-db-password|als-siphon|Password for PostgreSQL DSN for ALS database (may be redirected to *postgres-password* - see [Database DSNs and their passwords](#dsns_and_passwords))|
|als-json-log-db-password|als-siphon|Password for PostgreSQL DSN for ALS JSON logs database (may be redirected to *postgres-password* - see [Database DSNs and their passwords](#dsns_and_passwords))|
|flask-secret-key|rat-server, msghnd (deprecated, not really used), objst (not really used), cert-db (not really used)|Value for Flask's SECRET_KEY (used to cookie signing, CSRF token generation, etc.). Only used by *rat-server*, other Flask-based containers require it for initialization code not to crash|
|google-apikey|rat-server, msghnd (deprecated, not really used), objst (not really used), cert-db (not really used)|Google API key. Passed to WebUI for map drawing. Only used by *rat-server*, other Flask-based containers require it for initialization code not to crash|
|ratapi-captcha-secret|rat-server, msghnd (deprecated, not really used), objst (not really used), cert-db (not really used)|Secret for Google's `recaptcha`. Only used by *rat-server*, other Flask-based containers require it for initialization code not to crash|
|ratapi-captcha-sitekey|rat-server, msghnd (deprecated, not really used), objst (not really used), cert-db (not really used)|Passed to WebUI for captcha generation. Only used by *rat-server*, other Flask-based containers require it for initialization code not to crash|
|ratapi-mail-password|rat-server, uls-downloader, msghnd (deprecated, not really used), objst (not really used), cert-db (not really used)|SMTP password to use when sending mail. Only used by *uls-downloader* and *rat-server*, other Flask-based containers require it for initialization code not to crash|
|oidc-client-id|rat-server, msghnd (deprecated, not really used), objst (not really used), cert-db (not really used)|Client ID for OIDC-based (single sign-in, such as `Okta` - if used) login. Only be used by *rat-server*, other Flask-based containers require it for initialization code not to crash|
|oidc-client-secret|rat-server, msghnd (deprecated, not really used), objst (not really used), cert-db (not really used)|Client Secret for OIDC-based (single sign-in, such as `Okta` - if used) login. Only be used by rat-server, other Flask-based containers require it for initialization code not to crash|
|server-cert-pem|dispatcher|Server private key for TLS termination. Combined with *server-key-pem* into a single *server-cert* secret in cluster secret store|
|server-key-pem|dispatcher|Server certificate for TLS termination. Combined with *server-cert-pem* into a single *server-cert* secret in cluster secret store|
|grafana-db-password|grafana|Password for PostgreSQL DSN for Grafana database (may be redirected to *postgres-password* - see [Database DSNs and their passwords](#dsns_and_passwords))|
|als-ro-db-password|grafana|Password for PostgreSQL DSN for read-only access to ALS database (may be redirected to *postgres-password* - see [Database DSNs and their passwords](#dsns_and_passwords))|
|als-json-log-ro-db-password|grafana|Password for PostgreSQL DSN for read-only access to ALS JSON logs database (may be redirected to *postgres-password* - see [Database DSNs and their passwords](#dsns_and_passwords))|
|uls-state-ro-db-password|grafana|Password for PostgreSQL DSN for read-only acces to database with ULS Downloader state (may be redirected to *postgres-password* - see [Database DSNs and their passwords](#dsns_and_passwords))|
|grafana-admin-password|grafana|Grafana admin user password|


### Database DSNs and their passwords <a name="dsns_and_passwords">

As of time of this writing OpenAFC uses usernames and passwords in `PostgreSQL` database authentication. Passwords are parts of `PostgreSQL` connection strings (`DSN`s).

Passwords have to be stored in secrets, whereas `DSN`s better be stored less restrictively. Thus, all connection strings passed to OpenAFC use default passwords (usually `postgres`), actual passwords being supplied by means of secrets on `GCP` and not at all on `k3d` (i.3. `k3d` uses default passwords).


## Helmcharts <a name="helmcharts">

### File tree <a name="file_tree">

Everything pertinent to Kubernetes is defined in `helm` directory that has the following structure:
```
helm
+-- afc-common
|   \-- templates
|       \-- _helpers.tpl
+-- afc-ext
|   +-- templates
|   |   \-- *.yaml
|   +-- values.yaml
|   \-- values-*.yaml
+-- afc-int
|   +-- templates
|   |   \-- *.yaml
|   +-- values.yaml
|   \-- values-*.yaml
+-- bin
|   +-- push_images.py
|   +-- helm_install_*.py
|   +-- install_prerequisites.py
|   +-- k3d_cluster_create.py
|   +-- k3d_ports.py
|   \-- ... and other files, used indirectly
+-- monitoring
|   +-- prometheus
|       \-- *.yaml
|   +-- prometheus_adapter
|       \-- *.yaml
|   \-- prometheus_ingress
|       \-- *.yaml
\-- README.md
```

|Directory|Content|
|----------|-------|
|helm/afc-common|Stuff common to all clusters. For now - `helm/afc-common/templates/_helper.tpl` with macros used in helmchart rendering|
|helm/afc-int|Main (default) `values.yaml` of internal `GCP` and of `k3d` clusters and its overrides for various specific purposes|
|helm/afc-int/templates|Helmcharts of internal `GCP` and of `k3d` clusters|
|helm/afc-ext|Main (default) `values.yaml` of external `GCP` cluster and its overrides for various specific purposes|
|helm/afc-ext/templates|Helmcharts of external `GCP` cluster|
|helm/bin|Scripts for preparing/starting/stopping/restarting clusters. Maybe in future will be replaced by e.g. `Ansible` playbooks|
|helm/monitoring|Stuff related to `Prometheus`|
|helm/monitoring/prometheus|Prometheus manifests|
|helm/monitoring/prometheus_adapter|Rules for `Prometheus adapter` helmchart, used in worker autoscaling|
|helm/monitoring/prometheus_ingress|`Ingress-nginx` rules to export `Prometheus` outside of internal cluster. Exporting `Prometheus` was a temporary desperate measure, will be removed in the future|


### Manifests <a name="manifests">
OpenAFC is big and heterogeneous project (more than 60 helmcharts of 9 different types), hence for the sake of simplicity actual definition of everything is in `helm/afc-int/values.yaml` and `helm/afc-ext/values.yaml` and their overrides. (Almost) all helmcharts look pretty simplistic - e.g. here is a sample service helmchart:

<small>

```
{{- $ := mergeOverwrite $ (dict "component" "objst") }}
apiVersion: v1
kind: Service
metadata:
  name: {{ include "afc.hostName" . }}
  labels: {{- include "afc.commonLabels" . | nindent 4 }}
  {{- include "afc.serviceAnnotations" . | nindent 2 }}
spec:
  {{- include "afc.serviceIp" . | nindent 2 }}
  selector: {{- include "afc.serviceSelector" . | nindent 4 }}
  ports: {{- include "afc.servicePorts" . | nindent 4 }}
```

</small>

Couple of important aspects to note here:
- The only unique element in this helmchart (that differs it from other helmchart of same type) is *"objst"* on the first line, that denotes which part of `values.yaml` should be used to render this manifest. Some manifests are little more convoluted to allow conditional exclusion of the entire manifest (again, based on content of `values.yaml`).
- *"afc....* macros, included here are defined in `helm/afc-common/templates/_helper.tpl` (used for bot internal and external clusters)
- Helmchart content is granular enough to replace individual sections verbatim (without resorting to helm tricks). This is acceptable as temporary emergency measure, but the intention is to have everything individual to be defined in `values.yaml`


### `values.yaml` organization <a name="values_organization">

Detailed and up to date documentation to `values.yaml` contained inside comments in these files, so here is an just information on their top-level structure for navigation purposes:

|Top level subdictionary|Contents|
|-----------------------|--------|
|externalIps|External IPs of internal and external `GCP` clusters. Filled in site-specific `values.yaml` override|
|imageRepositories|External image registries ('repositories' is a misnomer, but let it be) of OpenAFC own pods|
|securityContexts|Security contexts used by OpenAFC own pods|
|sharedEnv|Blocks of environment variables, shared by several pods. Also - values (not necessary environment variables) that might be used in other environment variables. Comments describe a `Jinja`-like 'language' used in environment variables to share values defined elsewhere - this 'language' completely implemented in `_helpers.tpl`, without any third-party tools. Note that individual environment variables may also be defined in pod definitions (in *components* section), but shared values - only in this *sharedEnv* section|
|components|Dictionary that contains subdictionary for each OpenAFC 'component' (pod). Component definition includes source data for deployment/statefulset (image, environment, secrets, mounts), service (if needed), HPA (if needed), podmonitor (`Prometheus` exporting, if needed), ingress (if needed), etc. manifests|
|secretStores|Secret store definition. Should be overridden in site-specific `values.yaml` override|
|externalSecrets|Secrets definitions|


### `values.yaml` bulk overrides <a name="values_overrides">

Some values in `values.yaml` may be overridden in bulk (by means of YAML file) or individually. There are following types of bulk override files.


#### Feature-specific bulk overrides <a name="values_overrides_features">

Overrides used by various `helm/bin/helm_install_*.py` command line switches:

|File|Use|
|----|---|
|helm/afc-int/values-k3d.yaml|Implicitly used by `helm_install_int.py` when target cluster is `k3d`|
|helm/afc-int/values-http.yaml|Used by `helm_install_int.py --http` to enable HTTP on *dispatcher* on `k3d` platform|
|helm/afc-int/values-mtls.yaml|Used by `helm_install_int.py --mtls` to enable mTLS on *dispatcher* on `k3d` platform (default is TLS-only when HTTPS is used)|
|helm/afc-int/values-devel.yaml|Used by `helm_install_int.py --devel` to enable development features in various containers|
|helm/afc-int/values-msghnd.yaml|Used by `helm_install_int.py --msghnd` to use legacy *msghnd* instead of recently introduced *afcserver* to handle incoming AFC requests|
|helm/afc-int/values-no_secrets.yaml|Used by `helm_install_int.py --no_secrets` to enable operation on `k3d` platform without secrets (limited, but still quite functional)|
|helm/afc-ext/values-http.yaml|Used by `helm_install_ext.py --http` to enable HTTP on external cluster's *dispatcher*|
|helm/afc-ext/values-mtls.yaml|Used by `helm_install_ext.py --mtls` to enable mTLS on external cluster's *dispatcher* (default is TLS-only when HTTPS is used)|
|helm/afc-ext/values-devel.yaml|Used by `helm_install_ext.py --devel` to enable development features in *dispatcher*|


#### Site-specific, cluster-specific bulk overrides <a name="values_overrides_cluster_specific">

Since these overrides are site-specific, they are not on GitHub. They are applied with `--values` switch of `helm/bin/helm_install_*.py`.

Since there are two clusters (and two `values.yaml` files) on `GCP` platform, there should be two such override file. See [Deployment](#deployment) for specific recommendations on content of such files.


#### Site-specific, common bulk overrides <a name="values_overrides_site_specific">

On `GCP` platform some settings are shared/common between internal and external clusters: external IPs (as *dispatcher* of external cluster uses IPs of internal one), image registries, secret store, maybe something else.

YAML file with such settings also might be used as an override on `helm/bin/install_prerequisites.yaml` (e.g. external IP addresses might be used in `ingress-nginx` annotations).


## Scripts <a name="scripts">

Harnessing that many moving parts requires some scripting. For now these are `Python` scripts, maybe they'll be replaced by Ansible playbooks in the future.

All scripts can be found in `helm/bin` directory. When executed with `--help` parameter every script prints help message.


### The **`AUTO`** name <a name="auto">

Scripts described below create a lot of entities, each such entity requires name that is typically of no particular significance. Some scripts accept *AUTO* as a name and convert it to combination of current user name and root directory of OpenAFC sources (checkout directory).

On `k3d` platform this allows to have a name tied to a particular cluster (as even same user running several clusters in parallel will, most likely, run them from different checkout directories).


### `k3d_registry.py` <a name="k3d_registry_py">

For `k3d` platform only. 

Starts/stops `k3d` image registry.

`helm/bin/k3d_registry.py <OPTIONS>`

|Option|Meaning|
|------|-------|
|--delete|Delete (stop) `k3d` image registry. Default is create (start) registry|
|--force|On creation: start image registry even though some image registry already running. On deletion - if more than one registry running and no name is provided - delete them all|
|--registry **NAME**|Name of registry to create/delete. Default is to create *afc-registry.localhost* registry and delete whatever registry is running|

When running without parameters this script starts `afc-registry.localhost` registry if no `k3d` registries currently running, does nothing if some already running.


### `k3d_cluster_create.py` <a name="k3d_cluster_create_py">

For `k3d` platform only. 

Thick wrapper around `k3d cluster create`. It (re)creates ((re)starts) `k3d` cluster, installs prerequisite components, maps host's directory with static files (geospatial data, etc.) into it, connects to `k3d` image registry (started preliminarily with `k3d_registry.py`), exposes ports of some future pods, etc. Also it can delete (stop) `k3d` cluster.

If cluster with given name already running - it is deleted first. I.e. cluster is always created from scratch.

`helm/bin/k3d_cluster_create.py <OPTIONS> <CLUSTER NAME>`

|Parameter|Meaning|
|---------|-------|
|--delete|Delete (stop) `k3d` cluster. Default is to create (start) it|
|--static **DIRECTORY**|Host directory containing static files (geospatial databases, FS databases, etc.) If unspecified - well-known locations are explored (as of time of this writing `/opt/afc/databases/rat_transfer` is the only one well known location)|
|--k3d_reg **REGISTRY**|Name or **[NAME]:[PORT]** of `k3d` image registry to use. Default is to use name of (the only) `k3d` registry currently running|
|--expose **NODEPORT1[:HOST_PORT1][,NODEPORT2[:HOST_PORT2]]...**|Expose known nodeports as `localhost` ports. Known nodeports are taken from `helm/afc-int/values.yaml`, they can be seen in help message of this script. Host ports either specified explicitly, or unused ones allocated. This parameter may be specified more than once. Nodeport exposure has an effect similar to `kubectl port-forward`, but does not require separate terminal window. It can be used to peek inside the cluster: `pgAdmin` to peek into `PostgreSQL` databases, browser to peek into `Prometheus`, `curl` to play with various internal REST interfaces, etc.|
|--kubeconfig **CONFIGFILE**|Kubeconfig file to store cluster information in. Default is to use current, but when executed from, say, `kubie`, current config file is temporary, so explicit name should be specified|
|--namespace **NAMESPACE**|Default namespace in created cluster. Default is *default* namespace|
|**CLUSTER NAME**|Cluster (and kubeconfig context) name or *AUTO* (see [here](#auto)). Latter is recommended|


### `k3d_ports.py` <a name="k3d_ports_py">

For `k3d` platform only. 

Print port what cluster ports mapped to what `localhost` ports.

`helm/bin/k3d_ports.py <CLUSTER NAME>`

Here **CLUSTER NAME** is either cluster name (`k3d cluster list` to print them all), or  *AUTO* (see [here](#auto))


### `push_images.py` <a name="push_images_py">

Push images into registry, may optionally build them before pushing (using `tests/regression/build_imgs.sh`).

On `k3d` registry is detected automagically, on `GCP` registry should be specified explicitly.

Some installations (e.g. ones using `Artifactory`) may have different push registry (registry where to built images should be placed) and pull registry (registry where from Kubernetes pulls images) - this is also supported.

`helm/bin/push_images.py <OPTIONS> <TAG>`

|Parameter|Meaning|
|---------|-------|
|--build|Build images before pushing|
|--k3d_reg **REGISTRY**|Push to given `k3d` registry, that can be specified either as **[NAME][:PORT]**, or as *DEFAULT* (that autodetects a single `k3d` registry currently running). Latter is recommended (unless for some strange reason more than one `k3d` registry is running)|
|--registry **REGISTRY**|Host and port of registry Kubernetes will pull images from|
|--registry **REGISTRY**|**HOST[:PORT]** port of registry Kubernetes will pull images from|
|--push_registry **REGISTRY**|**HOST[:PORT]** port of registry to push images to. By default - same as specified with `--registry`|
|**TAG**|Tag of Docker images to push (and, maybe, build): tag name or *AUTO* (see [here](#auto))|


### `install_prerequisites.py` <a name="install_prerequisites_py">

Install prerequisites (all or some), mentioned in [Prerequisite components](#prerequisites) chapter.


#### Config file

`helm/bin/install_prerequisites.py` has accompanying configuration file `helm/bin/install_prerequisites.yaml` that describes how to install prerequisite components that need to be installed. See comments in `install_prerequisites.yaml` for the most up to date information of its semantics.

Similarly to `values.yaml`, `install_prerequisites.yaml` supports some `Jinja`-like formatting and similarly, it is not a real `Jinja`, as everything is implemented in `install_prerequisites.py`.

Similarly to `values.yaml`, `install_prerequisites.yaml` may be partially overridden (e.g. to add `ingress-nginx` annotations) and/or expanded (e.g. to add `Istio` labels to namespaces). This overriding implemented in `install_prerequisites.py` by merging YAML files (overriding individual settings from command line is not implemented). Merging is performed according to the following rules, applied recursively:

- If dictionary overrides dictionary, items from overriding dictionary add/replace items in dictionary being overridden.
- If list overrides list, items from overriding list **appended** to list being overridden.
- In all other cases, overriding entity replaces entity being overridden. For example, to eliminate list, it might be overridden with `null` (as overriding with empty list will have no effect)

It is convenient to use [sitewide helm settings file](#site_specific_common_bulk_overrides) as one of an overriding config files - values from there (e.g external IPs) may be fetched by `Jinja`-style magic (see comments in `install_prerequisites.yaml` for details) - see [`GCP` config override files](#gcp_config_overrides) for examples.


#### Invocation 

`helm/bin/install_prerequisites.py <OPTIONS> [<COMPONENTS>]`

|Parameter|Meaning|
|---------|-------|
|--cfg **CONFIG FILE**|Config file. More than one may be specified (then first is used as main, subsequent used as overrides). If none specified - `helm/bin/install_prerequisites.yaml` is used. Typically this parameter need not be specified: `helm/bin/install_prerequisites.yaml` most likely is good enough as main config, override configs may be specified via *--extra_cfg*|
|--extra_cfg **ADDITIONAL CONFIG FILE**|Config file override. More than one may be specified, applied in given order|
|--uninstall|Uninstall prerequisites (default is install)|
|--context **[KUBECONFIG_FILE][:CONTEXT]**|Kubeconfig and kubecontext of cluster to install prerequisites in. Context name may contain wildcards (e.g. *\*ext* - don't forget to properly quote such value) if actual context name is unbearably long. Default is to use current context of current kubeconfig|
|--print_cfg|Print config, resulting from applying of all overrides|
|**[COMPONENT1 COMPONENT2...]**|Components to install/uninstall. Names are keys of config file's *install_components* dictionary. If components unspecified - config file's *default_install_components* list is used (empty in default config, see [`GCP` config override files](#gcp_config_overrides) for overriding examples). If even this list is empty - all components are installed/uninstalled|


### `helm_install_int.py` <a name="helm_install_int_py">

Installs/uninstalls OpenAFC in internal (`GCP` platform) or `k3d` cluster. Helmcharts of this project are in `helm/afc-int`

`helm/bin/helm_install_int.py <OPTIONS> <RELEASE>`

|Parameter|Meaning|
|---------|-------|
|--uninstall|Uninstall OpenAFC helm project (default is to install)|
|--tag **TAG**|Tag of images to use. On `k3d` platform may also be *AUTO* (see [here](#auto)). Default is to use `Chart.yaml`'s *appVersion* (which might be a good choice for `GCP` platform)|
|--context **[KUBECONFUG][:KUBECONTEXT]**|Kubeconfig and kubecontext of cluster to install helm project in. Context name may contain wildcards (e.g. *\*.int* - don't forget to properly quote such value) if actual context name is unbearably long. Default is to use current context of current kubeconfig
|--namespace **NAMESPACE**|Namespace to install helm project in. Default is *default*|
|--build|Build and push images to registry before installing helm project|
|--push|Push images to registry before installing helm project|
|--push_registry **PUSH_REGISTRY**|Registry to push images to (on *--build* or *--push*). Default is to use one, specified by *--registry* or *--k3d_reg*|
|--registry **PULL_REGISTRY**|Registry Kubernetes will use to pull images form|
|--k3d_reg **[HOST][:PORT]**|`k3d` image registry to use. May also be specified as *DEFAULT* - to automagically detect currently running `k3d` registry|
|--fake_secrets **[SECRETSTORE][:FILE_OR_DIR]**|Creates `fake` backend for `ExternalSecret`. **SECRETSTORE** is a name of secret store (key in *secretStores* of `values.yaml`) - default is the first and only key. **FILE_OR_DIR** is a file or directory containing manifests of secrets - default is `tools/secrets/templates` that contains default/empty values for all secrets. For use on `k3d` platform|
|--no_secrets|Use no secrets (for `k3d` platform only). Uses `values.yaml` override file `helm/afc-int/no_secrets.yaml`|
|--http|Allow HTTP traffic on *dispatcher*, running in internal cluster. For `k3d` platform only, as on `GCP` platform *dispatcher* runs it in external cluster. Uses `values.yaml` override file `helm/afc-int/html.yaml`|
|--mtls|Enable mTLS in *dispatcher*, running in internal cluster. For `k3d` platform only, as on `GCP` platform *dispatcher* runs it in external cluster. Uses `values.yaml` override file `helm/afc-int/mtls.yaml`|
|--devel|Enable development features in various OpenAFC containers. Uses `values.yaml` override file `helm/afc-int/devel.yaml`|
|--msghnd|Uses *msghnd* for AFC requests' processing (default is to use *afcserver*). Uses `values.yaml` override file `helm/afc-int/msghnd.yaml`|
|--expose **NODEPORT_NAME1,NODEPORT_NAME2,...**|Convert given ports to NodePorts for exposition. Name list might be seen in script's help message. On `k3d` ports exposed in `k3d_cluster_create.yaml` are exposed automagically|
|--wait **TIMEunit**|Wait this amount of time (e.g. *5m*, *30s*, etc.) for completion of each major step|
|--preload_ratdb|Run `helm/bin/preload_ratdb.py` to fill RatDB with same initial stuff (users, AFC Configs, certificates) as `tests/regression/run_srvr.py` does - for use on `k3d` platform only|
|--upgrade|If OpenAFC helm already running, does `helm upgrade` instead of `helm uninstall`/`helm install`. Doesn't work very well|
|--max_workers **MAX_WORKER_PODS**|Maximum number of *worker* (`AFC Engine`) pods. Default is as many as to have one CPU per `AFC Engine` (worker pod by default runs two `AFC Engines`) - which might be fine on `k3d` but not on `GCP` platform. This value used as *worker*'s HPA upper boundary|
|--values **VALUES_YAML_FILE**|`values.yaml` override file. This parameter may be specified more than once|
|--set **VA.RI.AB.LE=VALUE**|`values.yaml` setting override. Sequences of dictionary keys are dot-separated. This parameter may be specified more than once|
|--temp_dir **DIRECTORY**|Temporary directory. As of time of this writing only used for fake secrets generation (i.e. used for development environment only). If this parameter is specified, directory not removed at script completion|
|**RELEASE**|Helm release name or *AUTO* (see [here](#auto))|

### `helm_install_ext.py` <a name="helm_install_ext_py">

Installs/uninstalls OpenAFC in external `GCP` cluster (assuming prerequisites already installed with `install_prerequisites.py`). Helmcharts of this project are in `helm/afc-ext`

`helm/bin/helm_install_ext.py <OPTIONS> <RELEASE>`

|Parameter|Meaning|
|---------|-------|
|--uninstall|Uninstall OpenAFC project from external cluster (default is to install)|
|--tag **TAG**|Tag of images to use. Default is to use `Chart.yaml`'s *appVersion*|
|--context **[KUBECONFUG][:KUBECONTEXT]**|Kubeconfig and kubecontext of cluster to install helm project in. Context name may contain wildcards (e.g. *\*ext* - don't forget to properly quote such value) if actual context name is unbearably long. Default is to use current context of current kubeconfig|
|--http|Allow HTTP traffic on *dispatcher*. Uses `values.yaml` override file `helm/afc-ext/html.yaml`|
|--mtls|Enable mTLS in dispatcher. Uses `values.yaml` override file `helm/afc-int/mtls.yaml`|
|--devel|Enable development features in *dispatcher*. Uses `values.yaml` override file `helm/afc-ext/devel.yaml`|
|--wait **TIMEunit**|Wait this amount of time (e.g. *5m*, *30s*, etc.) for completion of each major step|
|--upgrade|If OpenAFC helm already running, does `helm upgrade` instead of `helm uninstall`/`helm install`. Doesn't work very well|
|--values **VALUES_YAML_FILE**|`values.yaml` override file. This parameter may be specified more than once|
|--set **VA.RI.AB.LE=VALUE**|`values.yaml` setting override. Sequences of dictionary keys are dot-separated. This parameter may be specified more than once|
|**RELEASE**|Helm release name or *AUTO* (see [here](#auto))|


## Deployment <a name="deployment">

Some simplistic deployment scenarios for `k3d` and `GCP` platforms.

### `k3d` deployment <a name="k3d_deployment">

#### Required software <a name="k3d_software">

Only Linux installation will be considered.

- **Docker**. Installation *requires* sudo privileges:  
  Inspiration: https://docs.docker.com/engine/install/  
  ```
  curl -fsSL https://get.docker.com -o get-docker.sh
  sudo sh get-docker.sh
  rm get-docker.sh
  ```
- **k3d** Install `k3d` executable to `$BIN` directory:  
  Inspiration:  https://k3d.io/v5.6.0/#install-script  
  ```
  curl -fsSL https://get.docker.com -o get-docker.sh
  [sudo] bash -c "wget -q -O - https://raw.githubusercontent.com/k3d-io/k3d/main/install.sh | K3D_INSTALL_DIR=$BIN bash"
  ```
  Requires `sudo` if `$BIN` is not user-writable
- **kubectl** Install `kubectl` executable to `$BIN` directory:  
  Inspiration: https://kubernetes.io/docs/tasks/tools/install-kubectl-linux/
  ```
  curl -LO "https://dl.k8s.io/release/$(curl -L -s https://dl.k8s.io/release/stable.txt)/bin/linux/amd64/kubectl"
  [sudo] install -o root -g root -m 0755 kubectl $BIN/kubectl
  rm kubectl
  ```  
  `install` requires `sudo` if `$BIN` is not user-writable. Inner `curl` obviously requires `bash` (or at least - not `[t]csh`), but can be easily decomposed.
- **helm**. Install `helm` executable to `$BIN` directory:  
  Inspiration: https://helm.sh/docs/intro/install/  
  ```
  wget https://get.helm.sh/helm-v3.14.0-linux-amd64.tar.gz
  tar -xzvf helm-v3.14.0-linux-amd64.tar.gz
  [sudo] mv linux-amd64/helm $BIN
  rm linux-amd64 helm-v3.14.0-linux-amd64.tar.gz
  ```  
  `mv` requires `sudo` if `$BIN` is not user-writable


#### File preparations <a name="k3d_files"/>

##### Static files <a name="k3d_staic_files"/>

Static files (geospatial data, FS database directory, etc.) should be placed somewhere. Default location is `opt/afc/databases/rat_transfer`.

Some inspiration regarding content of this directory can be found in `database_readme.md` and `README.md` in source root directory.

##### Secrets <a name="k3d_secrets">

This step may be skipped if extensive work with WebUI is not planned, as defaults may suffice.

Default secrets may be found in `tools/secrets/templates/afc_secrets.yaml`. Copy of this file with secrets properly filled in may be created and subsequently used in invocation of `helm/bin/helm_install_int.py`

##### `values.yaml` override <a name="k3d_values_override">

This step may be skipped if extensive work with WebUI is not planned, as defaults may suffice.

This `values-override.yaml` file sets variables, described in `Customization.md`. Here is how this file might be organized:

```
sharedEnv:
  flask-conf:
    FLASK_USER_EMAIL_SENDER_EMAIL: ...
  ratapi-configurator:
    REGISTRATION_APPROVE_LINK: ...
    REGISTRATION_DEST_EMAIL: ...
    REGISTRATION_DEST_PDL_EMAIL: ...
    REGISTRATION_SRC_EMAIL: ...
    MAIL_USERNAME: ...
    USE_CAPTCHA: "..."
    CAPTCHA_VERIFY: ...
    USER_APP_NAME: ...
  oidc-configurator:
    OIDC_DISCOVERY_URL: ...
```

#### Create (start) cluster <a name="k3d_start">

```
# Start k3d image registry. May be performed one time or each time - it is idempotent
helm/bin/k3d_registry.py
# Start cluster and prerequisites
helm/bin/k3d_cluster_create.py AUTO
# Build and push images, then install OpenAFC helmchart and prefill RatDB. Enable HTTP
helm/bin/helm_install_int.py --tag AUTO --build [--values values-override.yaml] \
  --preload_ratdb --fake_secrets "[:<SECRETFILE>]" --http --wait 5m AUTO
```
Here:
- `--values values-override.yaml` may be omitted if default WebUI settings are used (e.g if WebUI not used at all)
- `<SECRETFILE>` is a file created [here](#k3d_secrets). If it was not created (to use secrets from `tools/secrets/templates/afc_secrets.yaml`) - `--fake_secrets ""` would suffice.
- `--build` might be omitted if images are up to date

#### Exposed ports <a name="k3d_exposed_ports">

*Dispatcher*'s HTTP and HTTPS ports are mapped to `localhost`'s ports, that (along with other exposed ports) can be seen with:  
`helm/bin/k3d_ports.py AUTO`

#### Reload <a name="k3d_reload">

Reload OpenAFC helm project without restarting cluster and reloading perquisites:  
`helm/bin/helm_install_int.py --tag AUTO --build --http --wait 5m AUTO`  
As before, *--build* might be omitted if images are up to date


#### Stop (delete) cluster <a name="k3d_stop">

`helm/bin/k3d_cluster_create.py --uninstall AUTO`


### `GCP` deployment <a name="gcp_deployment">

#### Provisioning <a name="gcp_provisoning">

Initial provisioning of `GCP` clusters (two of them: external that runs *dispatcher* pod and internal that runs everything else - see [here](#clusters)) is out of scope of this document. Before starting OpenAFC following preparations should be made:

- **SQL**. `PostgreSQL+PostGIS` SQL resource should be allocated (one is enough). For sake of simplicity, below it would be demonstrated how to make all connection strings use same *postgres* user (albeit secret structure allows for more granular approach - see [here](#secrets)).  
  Besides obligatory *postgres* database, OpenAFC uses following databases: *AFC_LOGS*, *ALS*, *fbrat*, *fs_state*, *rcache*. Of them *fbrat* may require some prefill (copying content from some previous instance (if any), running `helm/bin/preload_ratdb.py` after cluster started, etc.), other will help themselves.

- **Secret Manager (aka `gcpsm`)**. Overview of secrets is [here](#secrets).

  - **gcpsm-secret** (secret for accessing GCP secret store) should be provisioned to clusters' secret stores. Refer <https://external-secrets.io/main/provider/google-secrets-manager/> for inspiration

  - **image-pull** (secret for pulling images from registry) should be put to `gcpsm`. See <https://kubernetes.io/docs/concepts/configuration/secret/#docker-config-secrets> or <https://kubernetes.io/docs/tasks/configure-pod-container/pull-image-private-registry/#registry-secret-existing-credentials> on how to prepare it

  - **postgres-password** (*postgres* user database password) should be created and put to `gcpsm`

  - **ratdb-password, uls-state-db-password, als-db-password, als-json-log-db-password**, as said above, will be skipped for simplicity.

  - **flask-secret-key, google-apikey, ratapi-captcha-secret, ratapi-captcha-sitekey, ratapi-mail-password, oidc-client-id, oidc-client-secret** should be created and put to `gcpsm`. Figuring them out is out of scope of this document (refer `Customization.md` for inspiration)

  - **server-cert-pem, server-key-pem** `Nginx` server certificate and private key. `tools/secrets/templates/afc_secrets.yaml` contains self-signed certificates as an illustration, they should not be used on clusters accessible from the Internet

- **IP Addresses**. Internal server uses two external IP addresses (one by `ingress-nginx`, another by `RabbitMQ`), external server uses one external IP address (by `Nginx` working on *dispatcher*)

- **NFS** Used to store static data files (mainly geospatial databases and `AFC Engine` parameter files) and resulting files of WebUI requests. Some inspiration may be found in `database_readme.md`.

- **Jumphost** Host from which Kubernetes cluster control is performed. Should have `kubectl` and `helm` installed, kubeconfig properly filled (with information about internal and external clusters). Also this clusters should have access to files in `helm` folder and to config override files described in next chapter.


#### Config override files <a name="gcp_config_overrides">

Files below are just examples, YMMV.

In all files below site-specific placeholders denoted with `<...>`. 

- **`values-afc-common.yaml`** Override file for `values.yaml` of internal and external cluster and for `install_prerequisites.yaml`.  
  ```
  externalIps:
    # External IP for ingress of internal cluster
    int-ingress: <Ingress IP of internal cluster>
    # External IP for RabbitMQ AMQP of internal cluster
    int-rmq: <RMQ IP of internal cluster>
    # External IP for dispatcher (Nginx) on external cluster
    ext-dispatcher: <Dispatcher IP of external cluster>
    # PostgreSQL+PostGIS server
    postgres: <GCP database server IP>
  
  imageRepositories:
    # Distinction between private and public registries is deprecated. Both entries should refer same registry
    public:
      path: <hostname of registry for 'public' images>
      pullSecrets:
        - image-pull
    private:
      path: <hostname of registry for 'private' images>
      pullSecrets:
        - image-pull
  
  secretStores:
    secret-store:
      provider:
        gcpsm:
          auth:
            secretRef:
              secretAccessKeySecretRef:
                name: gcpsm-secret
                key: secret-access-credentials
          projectID: <GCP project name>
  ```  
  For sake of automation additional fields may be added (r.g. *pushPath* for registries, etc.) - provided they don't conflict with existing structure.

- **`values-afc-int.yaml`** Override file for `values.yaml` of internal cluster  
  ```
  sharedEnv:
    flask-conf:
      FLASK_SQLALCHEMY_DATABASE_URI: postgresql://postgres@{{ip:postgres}}/fbrat
      FLASK_USER_EMAIL_SENDER_EMAIL: <see Customization.md>
    rcache-common:
      RCACHE_POSTGRES_DSN: postgresql://postgres@{{ip:postgres}}/rcache
    ratapi-configurator:
      REGISTRATION_APPROVE_LINK: <see Customization.md>
      REGISTRATION_DEST_EMAIL: <see Customization.md>
      REGISTRATION_DEST_PDL_EMAIL: <see Customization.md>
      REGISTRATION_SRC_EMAIL: <see Customization.md>
      MAIL_USERNAME: <see Customization.md>
      USE_CAPTCHA: "True"
      CAPTCHA_VERIFY: <see Customization.md>
      USER_APP_NAME: <see Customization.md>
    oidc-configurator:
      OIDC_DISCOVERY_URL: <see Customization.md>
  
  components:
    als-siphon:
      env:
        POSTGRES_INIT_CONN_STR: postgresql://postgres@{{ip:postgres}}/postgres
        POSTGRES_ALS_CONN_STR: postgresql://postgres@{{ip:postgres}}/ALS
        POSTGRES_LOG_CONN_STR: postgresql://postgres@{{ip:postgres}}/AFC_LOGS
    uls-downloader:
      env:
        ULS_SERVICE_STATE_DB_DSN: postgresql://postgres@{{ip:postgres}}/fs_state
    bulk-postgres:
      imageName: null
    ratdb:
      imageName: null
    nginxexporter:
      imageName: null
    dispatcher:
      imageName: null
    rmq:
      service:
        type: LoadBalancer
        annotations:
          networking.gke.io/internal-load-balancer-allow-global-access: "true"
          networking.gke.io/load-balancer-type: Internal
    afcserver:
      containerResources:
        requests:
          cpu: 1600m
      env:
        AFC_SERVER_RATDB_DSN: postgresql://postgres@{{ip:postgres}}/fbrat
        AFC_SERVER_GUNICORN_WORKERS: 2
      hpa:
        # These settings may require site-specific adjustments
        maxReplicas: 3
        metric:
          resource:
            name: cpu
            target:
              type: Utilization
              averageUtilization: 80
        behavior:
          scaleUp:
            stabilizationWindowSeconds: 0
          scaleDown:
            stabilizationWindowSeconds: 120
  
  staticVolumes:
    # Here IPs should be specified verbatim, using values from externalIps is not possible
    rat_transfer:
      nfs:
        server: <GCP NFS Server IP>
        path: <Path to static files' directory>
    pipe:
      nfs:
        server: <GCP NFS Server IP>
        path: <Path to pipe directory, used mostly for development purposes>
    objst-storage:
      nfs:
        server: <GCP NFS Server IP>
        path: <path to objstore directory>
  
  # Redirecting all password secrets to 'postgres' user password secret
  externalSecrets:
    ratdb-password:
      remoteSecretName: postgres-password
    uls-state-db-password:
      remoteSecretName: postgres-password
    rcache-db-password:
      remoteSecretName: postgres-password
    als-db-password:
      remoteSecretName: postgres-password
    als-json-log-db-password:
          remoteSecretName: postgres-password
  ```

- **`values-afc-ext.yaml`** Override file for `values.yaml` of external cluster  
  ```
  components:
    dispatcher:
      service:
        annotations:
          networking.gke.io/internal-load-balancer-allow-global-access: "true"
          networking.gke.io/load-balancer-type: Internal
  ```

- **`install-prerequisites-cfg-int.yaml`** Override file for `install_prerequisites.yaml` to use in internal cluster  
  ```
  default_install_components:
    - prometheus_operator
    - prometheus
    - prometheus_adapter
    - external_secrets
    - ingress_nginx
    - prometheus_ingress
  
  install_components:
    ingress_nginx:
      helm_settings:
        - controller.service.annotations."networking\.gke\.io/internal-load-balancer-allow-global-access"=true
        - contorller.service.internal.enabled="true"
        - controller.service.internal.annotations."cloud\.google\.com/load-balancer-type"=Internal
        - controller.service.annotations."networking\.gke\.io/load-balancer-type"=Internal
        - controller.service.loadBalancerIP={{externalIps.int-ingress}}
  ```  
  Note the `{{externalIps.int-ingress}}` construct used here. This is not `Jinja` or `Go` (aka `Helm`), it's implemented completely in `install_prerequisites.poy` and documented in `install_prerequisites.yaml`.

- **`install-prerequisites-cfg-ext.yaml`** Override file for `install_prerequisites.yaml` to use in external cluster  
  ```
  default_install_components:
    - external_secrets
  ```  

#### Putting images to registry <a name="gcp_push_images">

Registry login might be needed before push (it's not a part of command below).

`helm/bin/push_images.py --registry <REGISTRY> [--push_registry <PUSH REGISTRY>] [--build] <TAG>`

Here:
- **`<REGISTRY>`** - same registry as in `imageRepositories.public.path` and `imageRepositories.private.path` of `values.yaml`
- **`<PUSH REGISTRY>`** should be specified if different from `<REGISTRY>` (as it might be in case of `Artifactory`)
- **`--build`** builds images before push - this is optional.
- **`<TAG>`** Image tag or *AUTO* (see [here](#auto))

#### Installing prerequisites <a name="gcp_install_prerequisites"/>

Assuming clusters are created and fully provisioned.

```
helm/bin/install_prerequisite.py --extra_cfg values-afc-common.yaml \
  --extra_cfg install-prerequisites-cfg-int.yaml \
  --context <[KUBECONFIG][:CONTEXT_INT]>
helm/bin/install_prerequisite.py --extra_cfg values-afc-common.yaml \
  --extra_cfg install-prerequisites-cfg-ext.yaml \
  --context <[KUBECONFIG][:CONTEXT_EXT]>
```

Here:
- **`values-afc-common.yaml`, `install-prerequisites-cfg-int.yaml`, `install-prerequisites-cfg-ext.yaml`** Files created [previously](#gcp_config_overrides)
- **`KUBECONFIG`** Kubeconfig file. May be omitted if default
- **`CONTEXT_INT`, `CONTEXT_EXT`** Context names for internal and external clusters. May be omitted if default. May use wildcards (e.g. `*ext`)


#### Installing/reinstalling OpenAFC helmcharts <a name="gcp_install_helmcharts">

```
helm/bin/helm_install_int.py --tag <TAG>  --context <[KUBECONFIG][:CONTEXT_INT]> \
  --wait 5m --values values-afc-common.yaml --values values-afc-int.yaml openafc
helm/bin/helm_install_ext.py --tag <TAG>  --context <[KUBECONFIG][:CONTEXT_EXT]> \
  --wait 5m --values values-afc-common.yaml --values values-afc-ext.yaml openafc
```

Here:
- **`<TAG>`** Images' tag, used when doing `helm/bin/push_images.py` [above](#gcp_push_images)
- **`values-afc-common.yaml`, `values-afc-int.yaml`, `values-afc-ext.yaml`** Files created [previously](#gcp_config_overrides)
- **`KUBECONFIG`** Kubeconfig file. May be omitted if default
- **`CONTEXT_INT`, `CONTEXT_EXT`** Context names for internal and external clusters. May be omitted if default. May use wildcards (e.g. `*ext`)


#### Uninstalling OpenAFC helmcharts <a name="gcp_uninstall_helmcharts">

```
helm/bin/helm_install_int.py --uninstall --context <[KUBECONFIG][:CONTEXT_INT]> openafc
helm/bin/helm_install_ext.py --uninstall  --context <[KUBECONFIG][:CONTEXT_EXT]> openafc
```

Here:
- **`KUBECONFIG`** Kubeconfig file. May be omitted if default
- **`CONTEXT_INT`, `CONTEXT_EXT`** Context names for internal and external clusters. May be omitted if default. May use wildcards (e.g. `*ext`)

#### Uninstalling prerequisites <a name="gcp_uninstall_prerequisites">

```
helm/bin/install_prerequisite.py --uninstall --extra_cfg values-afc-common.yaml \
  --extra_cfg install-prerequisites-cfg-int.yaml \
  --context <[KUBECONFIG][:CONTEXT_INT]>
helm/bin/install_prerequisite.py --uninstall --extra_cfg values-afc-common.yaml \
  --extra_cfg install-prerequisites-cfg-ext.yaml \
  --context <[KUBECONFIG][:CONTEXT_EXT]>
```

Here:
- **`values-afc-common.yaml`, `install-prerequisites-cfg-int.yaml`, `install-prerequisites-cfg-ext.yaml`** Files created [previously](#gcp_config_overrides)
- **`KUBECONFIG`** Kubeconfig file. May be omitted if default
- **`CONTEXT_INT`, `CONTEXT_EXT`** Context names for internal and external clusters. May be omitted if default.
