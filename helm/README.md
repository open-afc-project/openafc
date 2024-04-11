Copyright (C) 2022 Broadcom. All rights reserved.\
The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate that
owns the software below. This work is licensed under the OpenAFC Project
License, a copy of which is included with this software program.

# Running AFC on Kubernetes (k3d and beyond)

## Table of contents
- [TL;DR](#tldr)
- [Overview](#overview)
- [Source code structure](#source_code_structure)
- [Names](#names)
- [Kubeconfigs, namespaces, clusters](#configs-namespaces-clusters)
- [K3d](#k3d)
  - [K3d installation](#k3d_installation)
  - [K3d and outer world](#k3d_in_a_world)
    - [K3d image registry](#k3d_image_registry)
    - [K3d host paths](#k3d_host_path)
    - [K3d ingress](#k3d_ingress)
- [Starting cluster](#starting_cluster)
  - [K3d](#k3d_starting_cluster)
- [Automation scripts](#scripts)
  - [`k3d_cluster_create.py`](#k3d_cluster_create)
  - [`k3d_helm_install.py`](#k3d_helm_install)
  - [`k3d_ports.py`](#k3d_ports)
  - [Other scripts](#other_scripts)

## TL;DR <a name="tldr">

- (Re)start `k3d` cluster (provided everything else prepared to it). `AUTO` as cluster name name it after login name and checkout directory. Cluster exposes AFC's HTTP/HTTPS and Prometheus ports:  
  `$ helm/bin/k3d_cluster_create.py --expose http,https,prometheus AUTO`
- Build images (`AUTO` as tag name makes tag named after login name and checkout directory), load images to k3d image registry; (re)load AFC into the current cluster. `AUTO` as helm release name makes release name from login name and checkout directory. In loaded AFC cluster HTTP is enabled, no secrets, HTTP/HTTPS/Prometheus ports exposed. Then ratdb prefilled from test database (similarly to what `run_srvr.sh` does):  
```
$ helm/bin/helm_install.py --tag AUTO --build --http --no_secrets \
    --wait 5m --preload_ratdb AUTO
```
- View external port mapping in current cluster:  
  `$ helm/bin/k3d_ports.py CURRENT`
- Delete cluster created above:  
  `$ helm/bin/k3d_cluster_create.py --delete AUTO`

In all examples above cluster/tag/release names may be changed from `AUTO` to something specific.

## Overview <a name="overview"/>

AFC server is organized as a set of docker containers, running under the control of some container orchestrator (that provides interconnection, resource allocation, etc.). Besides originally used `Docker Compose` orchestrator, AFC server may now work under the control of `Kubernetes`.

Full-scale `Kubernetes` operation is more suitable for cloud environments (such as `GCP` or `AWS`), whereas development better be performed on some scaled down `Kubernetes`-like orchestrator.

Among such scaled-down `Kubernetes` implementations the simplest is, probably `Minikube`. `Minikube` is maintained by the same community as 'big' `Kubernetes`, and it does the job well when it works. Unfortunately in some environments (e.g. on `CentOS 7`) it can't mount host drives as hostPath (`Minikube` uses `9p` mounting, not available by default in `CentOS 7` kernel).

Another alternative is `k3d`. `K3d` is maintained by different community, but it is made from Kubernetes sources and it works fine on `CentOS 7`. This document describes AFC cluster operation on `k3d`. Maybe in the future this document will be extended to `GCP` operation with full scale `Kubernetes`.

## Source code structure <a name="source_code_structure">

As of time of this writing all pertinent files located in `helm` directory that has the following structure:

```
helm
+-- afc-int
|   +-- Chart.yaml
|   +-- templates
|   |   +-- *.yaml
|   +-- values.yaml
|   +-- values-k3d.yaml
|   +-- values-gcp.yaml
|   +-- values-no_secrets.yaml
|   +-- values-http.yaml
+-- afc-ext
+-- afc-common
|   +-- Chart.yaml
|   +-- templates
|       +-- _helpers.tpl
+-- secrets
|   +-- *.yaml
+-- bin
|   +-- *.bin
+-- README.md
```
Here is what is important to be known about these folders and files:

* **afc-int**. Contains files that define 'inner' AFC cluster - cluster that defines all AFC functionality but not directly accessible from brutal outer world. Roughly corresponds to functionality of Docker Compose AFC project
  * **templates**. Contains helmcharts (patterns from which Kubernetes manifests are generated).  
    * **\*.yaml**. Helmcharts themselves.  
      Helmcharts of same type (deployment, configmap, etc.) are almost identical, they do not contain any literals (except those that designate to which component they represent). All literals are in `values.yaml` (and in files that override it).  
      Except for top-level stuff, all Kubernetes constructs are defined in macros of `helm/afc-common/templates/_helpers.tpl` (i.e. not present in helm charts verbatim).  
      Those who'll maintain these helmcharts - please keep it this way (literals in `values.yaml`, constructs in `_helpers.tpl`), as it greatly simplifies maintenance, compared to have repeated stuff spread over tens of files.
  * **Chart.yaml**. Helmcharts' version, images' version, references to other helmcharts and other top-level bookkeeping.  
    *But please don't forget to bump helmchart/images versions, stored in this file!*
  * **values.yaml**. The most important file! It contains default values for all literals to be used in Kubernetes manifests. Hope it is commented well enough - enjoy reading.  
    *Please note that this file contains **defaults*** And define data structure. All customization should be performed in platform/usecase/user-specific value files.
  * **values-*.yaml**. Platform/usecase-specific overrides of `values.yaml`.
* **afc-ext** Folder for forthcoming 'outer' AFC cluster.
* **afc-common** Folder for files common for all cluster-definition helmcharts
  * **templates/_helpers.tpl**. Contains macros, that render various fragments of helmcharts.
* **Secrets** Directory full of secret manifests that illustrate the structure of used secrets, but not secrets themselves.
* **bin** Various scripts, initially written to simplify `k3d` operation
* **README.md** This file

## Names <a name="names">

Helmchart file names for deployments, stateful sets and services follow naming convention, established in Docker Compose (see `tests/regression/docker-compose.yaml`). All deviations (e.g. use of `webui` instead of `rat_server`, names of docker images, etc.) are covered in `values.yaml`.

Kubernetes enforces **RFC1123** (`[a-z0-9-]+`) on names of its objects, hence all `Non_lowercase-names` (e.g. service names from `tests/regression/docker-compose.yaml`) are converted to `these-names`, whereas all `thatNames` (e.g. keys from `values.yaml`) converted to `non-lowercase-names` (so called **kabob case**). E.g. `deployment-uls_downloader.yaml` contains deployment named `uls-downloader`. This conversion performed transparently by macros, defined in `_helpers.tpl`, so `values.yaml` may use any name styles.

##### Dynamic names  <a name="dynamic_names">

Some generated Kubernetes objects in AFC cluster (most importantly - pods, but also, say, replicasets) have names with random suffix (e.g. `msghnd` pod may be named `msghnd-b6685c96b-sb99n`). To use such name in `kubectl` command one needs to find such name first (e.g. `$ kubectl get pods`). Alternatively one can install tab completions (`$ source <(kubectl completion bash)`) and use tab button to fill in the random part of name.

**However** Tab completions only available for `bash` and `zsh`, and on `bash` - only if `bash-completion` (aka `bash_completion`) module is installed (this installation requires `sudo` privileges and thus may be unavailable).


## Kubeconfigs, namespaces, clusters <a name="configs-namespaces-clusters"/>

Something that 'runs on Kubernetes' - runs in some **Kubernetes cluster**. Kubernetes cluster is an **API server** (entity accepting **Kubernetes manifests**, storing them and  and notifying subscribers on new/changed Kubernetes manifests), surrounded by surrounded by retinue of **Kubernetes Controllers** (entities subscribed to changes in manifests and doing the real job - well, mostly making changes in those manifests to elicit attention of other controllers).

Every **Kubernetes Object** (entity that has Kubernetes manifest - pod, service, deployment, etc.) has name. This name either cluster-global or defined in certain namespace. Cluster-global names (including, but not limited to, namespace names) are defined within a cluster, i.e. ther eis no clash if different clusters use the same name.

Default namespace is named `default`.

Cluster (defined by name and API Server address), current namespace and some other attributes comprise a **Kubernetes context**. The purpose of contetx is, as always, switching - switching between Kubernetes contexts allows one to switch between several clusters or between different namespaces in one cluster.

Kubernetes Contexts are stored in **Kubeconfig files** - each Kubeconfig may contain several contexts, one of contexts is current.

**Current Kubeconfig** (containing set of contexts, one of which is current :D ) is pointed to by **`KUBECONFIG`** environment variable. If this variable not defined - **default Kubeconfig** `~/.kube/config` is used.

Of course, for unsophisticated operation it is enough to always use default (and only) context in default kubeconfig.

Some useful commands:

* **View current kubeconfig**:  
  `$ kubectl config view`

* **View current context** of current kubeconfig:  
  `$ kubectl config view --minify`

* **View current namespace** in current context of current kubeconfig:  
  `$ kubectl config view --minify | grep namespace:`

* **List existing namespaces**:  
  `$ kubectl get namespaces`

* **Create namespace**:  
  `$ kubectl create namespace NAMESPACE`

* **Delete namespace**:  
  `$ kubectl delete NAMESPACE`  
  *This operation deletes all objects, created in namespace*

* **Set current namespace** in the current context of the current kubeconfig:  
  `$ kubectl config set-context --current --namespace=NAMESPACE`

Most `kubectl` and `helm` commands allow to explicitly specify namespace (with `--namespace` or `-n` switch). Cluster and API Server on the other hand are typically taken from the current context of the current kubeconfig.

In <https://home.robusta.dev/blog/switching-kubernets-context> one can find a review of tools for juggling contexts and namespaces. Briefly:
* **kubectx** and **kubens** (<https://github.com/ahmetb/kubectx>). Update current kubeconfig and hence make global (common to all terminal windows) changes.
* **kubie** (<https://github.com/sbstp/kubie>). Spawns shell and create temporary kubeconfig for it (hence does not affect other terminal windows). Prints namespace and cluster names in prompt.  
  Works great with `bash`, but doesn't work in `tcsh`.
* **kubeswitch** (<https://github.com/danielfoehrKn/kubeswitch>) Something created for MacOS and ported to Linux.  
  Doesn't seems like really work on Linux (YMMV).


## K3d <a name="k3d">

**k3d** is a stripped-down **k3s** which is, in turn, stripped-down Kubernetes. They share underlying code (albeit each step involved removal of some code) and are all community-supported. Heritage is important, as some information on `k3d` may be found only in `k3s` form.

`K3d` is a single executable. It supports multicluster/multinode/docker-only operation. It is significantly more complicated than `Minikube` and significantly less documented than `Kubernetes`. Still it runs even on `CentOS 7` - and this is good.

### K3d installation <a name="k3d_installation">

K3d operation requires `kubectl`, `helm` and`k3d` to be installed

- **Kubectl**. Instructions may be found here: <https://kubernetes.io/docs/tasks/tools/install-kubectl-linux/> (for macOS: <https://kubernetes.io/docs/tasks/tools/install-kubectl-macos/>) - essentially it is downloading binary and copying it to some bin directory. Ubuntu installation to `/usr/local/bin`:  
  `$ curl -LO "https://dl.k8s.io/release/$(curl -L -s https://dl.k8s.io/release/stable.txt)/bin/linux/amd64/kubectl"`  
  `$ sudo install -o root -g root -m 0755 kubectl /usr/local/bin/kubectl`  
  `$ rm kubectl`  
  It can be installed without `sudo` to e.g. `~/.local/bin`
- **Helm**. Instructions may be found here: <https://helm.sh/docs/intro/install/>. It is also downloading binary and copying it to some bin directory.  Ubuntu installation to `/usr/local/bin`:  
  `$ wget https://get.helm.sh/helm-v3.14.0-linux-amd64.tar.gz`  
  `$ tar -xzvf helm-v3.14.0-linux-amd64.tar.gz`  
  `$ sudo install -o root -g root -m 0755 linux-amd64/helm /usr/local/bin/helm`  
  `$ rm -rf linux-amd64 helm-v3.14.0-linux-amd64.tar.gz`  
  It can be installed without `sudo` to e.g. `~/.local/bin`
- **K3d**. Instructions may be found here: <https://k3d.io/v5.6.0/#installation>. Default installation is performed to `/usr/local/bin` and goes like this:  
  `$ wget -q -O - https://raw.githubusercontent.com/k3d-io/k3d/main/install.sh | bash`  
  Installation to different directory and/or without `sudo`. E.g. here is installation to ~/.local/bin:  
  `$ wget https://raw.githubusercontent.com/k3d-io/k3d/main/install.sh`  
  `$ chmod +x install.sh`  
  ``$ env USE_SUDO=false K3D_INSTALL_DIR=`realpath ~/.local/bin` ./install.sh``  
  `$ rm install.sh`  
  Note that `realpath` invocation above, it is there because simple `~/.local/bin` doesn't work.

### K3d and outer world <a name="k3d_in_a_world">

`K3d` cluster relations with outer world are peculiar (compared to 'big' `Kubernetes`).

#### K3d image registry <a name="k3d_image_registry">

`K3d` only can grab images from external docker repositories. Using remote repositories is rather inconvenient for development (in terms of both speed and fuss), so `k3d` provides an option to create local registry (aka repository) - as described here <https://k3d.io/v5.2.0/usage/registries/>

Creating local docker registry:  
`$ k3d registry create afcregistry.localhost [--port 12345]`  
This command creates `k3d-afcregistry.localhost:12345` registry that uses port `12345` on `localhost` (note the `k3d-` prefix) and prints brief explanation how to use it. Port specification is optional - if unspecified it is dynamically selected.

Note that once created registry may be left alone - there is no need for each user/cluster to have its own registry, one for all is enough. Supplied scripts  k3d local registry, provided just one is running.

This registry might be used on k3d cluster creation:  
`$ k3d cluster create ... --registry-use k3d-afcregistry.localhost:12345`

Registry may be removed with:  
`$ k3d registry delete afcregistry.localhost:12345`

Registries may be listed:  
`$ k3d registry list`  
or with details (e.g. port number):  
`$ k3d registry list -o yaml`

Magic that prints port number of first registry:  
`$ echo $(k3d registry list -o json | jq -r 'getpath([0,"portMappings","5000/tcp",0,"HostPort"])')`

##### Putting images to registry

Standard method to put `foo/bar:baz` image to registry created above is:  
```
$ docker tag foo/bar:baz k3d-afcregistry.localhost:12345/bar:baz
$ docker push k3d-afcregistry.localhost:12345/bar:baz
```

**However** Unfortunately this would not work on `CentOS 7` - because it can't resolve `k3d-afcregistry.localhost` (or whatever else) local registry hostname (whereas Ubuntu can). Still `CentOS 7` may access this registry as `localhost:12345` (`k3d` - can't as inside container it has its very own localhost). Procedure for this unfortunate case:  
```
$ docker tag foo/bar localhost:12345/bar:baz                  # For push to work
$ docker tag foo/bar k3d-afcregistry.localhost:12345/bar:baz  # For pull to work
$ docker push localhost:12345/bar:baz
```

This procedure may be performed with `helm/bin/k3d_push_images.py` script. It is also may be performed as part of `helm/bin/helm_install.py` script (if `--build` or `--push` parameter specified).

Images in local k3d registry have registry names like `k3d-afcregistry.localhost:12345` that differ from original registry names of these images (as used in Docker Compose) - this should be reflected in deployment/statefulset manifests. `values.yaml` has `components.default.imageRepositoryKeyOverride` field to override what registry name to use in images and `imageRepositories.k3d.path` to set the name of k3d registry. Hence helm install command should be like this:  
`$ helm install ... helm/afc-int ... -f helm/afc-int/values-k3d.yaml --set imageRepositories.k3d.path=k3d-afcregistry:12345`  
Here `values-k3d.yaml`, among other things, sets `components.default.imageRepositoryKeyOverride` to `k3d`, whereas `--set` parameter sets path to registry.


#### K3d host paths <a name="k3d_host_path">

`K3d` API Server runs inside the container, so all prospective hostPath directories should be mounted to cluster at cluster creation:  
`$ k3d cluster create ... -v /opt/afc/databases:/opt/afc/databases -v /home:/hosthome ...`  
Here venerable `/opt/afc/databases` is mounted because some containers use it, whereas `/home` is mounted for those who inclined to use `/pipe` sometimes.

#### K3d ingress <a name="k3d_ingress">

<https://k3d.io/v5.4.6/usage/exposing_services/> provides working examples of ingress implementations with very little background explanation (e.g. no convincing explanation for the role of agents and proper ranges of NodePort ports).

`values-k3d.yaml` follows NodePort example that should be used like this:
* **Pick unused ports**.  
  Used ports may be seen in `netstat -latn` output, unused are those that not used.  
  Let's name them `$AFC80` and `$AFC443`

* **Start cluster** mapping these ports to `30080` and `30443` respectively (these two values may be found in `values.yaml` in `components/dispatcher/ports/*/nodePort`):  
  `$ k3d cluster create ... -p "$AFC80:30080@agent:0" -p "$AFC443:30443@agent:0" --agents 2`

* **Load AFC into cluster** using `values-k3d.yaml`:  
  `$ helm install ... helm/afc-int ... -f helm/afc-int/values-k3d.yaml...`


## Starting cluster <a name="starting_cluster"/>

### K3d <a name="k3d_starting_cluster">

This example shows creation of inner cluster only. Creation of outer cluster may be added some day.

* Make sure that **local image registry** [started](#k3d_image_registry):  
  `$ k3d registry list`  
  If not - start under whatever name and maybe even without specifying port. Name of local registry:  
  `$ LOCAL_REGISTRY=$(k3d registry list -o json | jq -r 'getpath([0,"name"])'):$(k3d registry list -o json | jq -r 'getpath([0,"portMappings","5000/tcp",0,"HostPort"])')`  
  Naturally, this address may be found in some less brutish way (e.g. by observing output of `$ k3d registry list -o yaml`

* **Create cluster** (if not yet).  
  In command below `$AFC80` and `$AFC443` are unused ports in 30000+ range to be used for HTTP and HTTPS communication with AFC. As said [above](#k3d_ingress), they can be found by observing `netstat -latn` output (that shows used ports).  
```
$ k3d cluster create my-cluster -v /opt/afc/databases:/opt/afc/databases -v /home:/hosthome \
    --registry-use LOCAL_REGISTRY -p "$AFC80:30080@agent:0" -p "$AFC443:30443@agent:0" --agents 2
```  

* Build images with some tag MYTAG and **push images to local repository**:  
  `$ helm/bin/k3d_push_images.py MYTAG`  

* **Load AFC** into cluster:  
  `$ helm dependencies update helm/afc-int`  
  `$ helm install RELEASE helm/afc-int -f helm/afc-int/values-k3d.yaml \`  
  `  [-f helm/afc/values-no_secrets.yaml] [-f helm/afc-int/values-http.yaml] \`  
  `  [--set components.default.imageTag=MYTAG]`  
  First command is only needed after `helm/afc-common/templates/_helpers.tpl` changed - but there is no harm in doing it every time.  
  Optional components of second command:
  * `values-no_secrets.yaml` - run without secrets. Related parts of AFC functionality will be unavailable 
  * `values-http.yaml` - run ingress in HTTP mode (don't mandate HTTPS)
  * `components.default.imageTag` - tag for used images. If omitted - `appVersion` from `helm/afc-int/Chart.yaml` will be used

* **Admire the result**:  
  `$ kubectl get all`  

* **Wait until AFC up and running**:  
  `$ kubectl wait --all --for=condition=Ready pod --timeout=300s`

* **Do the deed**:  
  `$ curl -d @afcReq.json localhost:$AFC80`  
  Here `$AFC80` is port, chosen during `k3d cluster create` (see above) for HTTP traffic.

* **Unload AFC from cluster**:  
  `$ helm uninstall RELEASE`
  
* **Remove cluster**:  
  `$ k3d cluster delete my-cluster`


### GCP <a name="gcp_starting_cluster">

Will come later.


## Automation scripts <a name="scripts">

Scripts located in `helm/bin` directory. Initially these scripts support `k3d` and, partially, `minikube` operation. Maybe later they'll be extended to GCP or AWS.

### `k3d_cluster_create.py` <a name="k3d_cluster_create">

Wrapper around `$ k3d cluster create`.

(Re)creates/deletes k3d cluster, along with eponymous kubecontext and, possibly, namespace. Created cluster also holds address of static file directory (usually `/opt/afc/databases/rat_transfer`) and 'front side' of host port mappings to ports of cluster services (`helm_install.py` may use some of these mappings).

`$ helm/bin/k3d_cluster_create.py [OPTIONS] CLUSTERNAME`

Here `CLUSTERNAME` may be cluster name or `AUTO`, that creates cluster name from login name and checkout directory name.

Without options created cluster has http and https ports of dispatcher mapped to host's unused ports.

Options:

|Option|Meaning|
|------|-------|
|**--delete**|Delete cluster. Default is (re)create it|
|**--expose** *NODEPORT1[:HOST_PORT1][,NODEPORT2[:HOST_PORT2]]...*|Cluster services' ports to map to host ports. Host ports may optionally be specified (by default unused ports in 30000+ range are used). Nodeports may be specified by name (see script's help message for list of known port names) or numerically|
|**--kubeconfig** *FILENAME*|Explicitly specifies kubeconfig file in which create cluster's context. Default is current kubeconfig. This parameter may be needed when executing from under `kubie` or other context switcher that uses temporary kubeconfig file|
|**--namespace** *NAMESPACE*|Namespace to use in cluster's context. Default is to use default namespace|
|**--static** *DIRECTORY*|Static files' directory. Useful if static files are in some unconventional place (usually they are in `/opt/afc/databases/rat_transfer`)|
|**--k3d_reg** *REGISTRY*|K3d image registry to use. Needed if more than one k3d registry running (which is not recommended). Default to use the first (and only) k3d registry or, if none running, not use it (load images from the remote registries)|
|**--cfg** *CFG_FILE*|Script's config fil ename. Default is `helm/bin/k3d_cluster_create.yaml`. May also be specified by means of `K3D_CLUSTER_CREATE_CONFIG` environment variable|

### `k3d_helm_install.py` <a name="k3d_helm_install">

Wrapper around `$ helm install`.

(Re)starts AFC inside the previously created cluster. As of time of this writing - only k3d clusters are supported. May also build/push images to k3d local registry and prefill `ratdb` from test database. 

`$ helm/bin/helm_install.py [OPTIONS] RELEASE`

Here `RELEASE` may be release name or `AUTO` that creates release name from login name and checkout directory.

Options:

|Option|Meaning|
|------|-------|
|**--tag** TAG|Tag of images to use. May be some tag name or `AUTO` that creates tag name from login name and checkout directory. If unspecified - `appVersion` from `Chart.yaml` is used (usually it corresponds to version, released to remote repositories)|
|**--build**|Build images and push them to local k3d registry before (re)starting the cluster. `--tag` must be specified|
|**--push**|Push images to local k3d registry before (re)starting the cluster|
|**--http**|Enable HTTP use on `dispatcher`. Default is HTTPS-only even if HTTP port is exposed|
|**--no_secrets**|Enables operation without secrets. By default secrets should be somehow loaded before cluster starts|
|**--wait** *TIMEOUT*|Wait for helm installation completion for given time (e.g. `5m`, `30s`, `5m30s` etc.)|
|**--preload_ratdb**|Preload ratdb from test database (like `tests/regression/run_srvr.sh` does). Requires `--wait`|
|**--upgrade**|If helmchart is already running - do `helm upgrade` (rolling update that preserves AFC operation continuity) and ignore `--preload_ratdb`. Default is to uninstall and (completely stop AFC) reinstall|
|**--values** *VALUES_FILE*|Additional `values.yaml` file (e.g. with user/usecase-specific settings). If specified without path - same directory as `values.yaml` is assumed. May be specified several times|
|**--set** *SE.TT.ING=VALUE*|Sets/overrides individual value for `values.yaml`. Hierarchy levels specified by dots|
|**--max_workers** *WORKERS*|Maximum number of worker pods. Nonpositive if as per helmchart. For k3d default is number of CPUs divided by number of AFC Engines per worker|
|**--expose** *NODEPORT1,NODEPORT2,...*|Nodeport names to expose. For k3d clusters, by default - same as exposed by cluster (`--erxpose` switch of `k3d_cluster_create.py`)|
|**--context** *[KUBECONFIG_FILE][:CONTEXT]*|Kubeconfig file and/or context to use. Default is current context of current kubeconfig|
|**--namespace** *NAMESPACE*|Namespace to use. Default is namespace from current (or chosen with `--context`) context|
|**--k3d_reg** *REGISTRY*|K3d registry to use. May be useful if more than one running (not recommended). If none running - none used (thus images are loaded from remote repositories)|

### `k3d_ports.py` <a name="k3d_ports">

Prints port mapping for given k3d cluster:

`$ helm/bin/k3d_ports.py CLUSTERNAME`

Here `CLUSTERNAME` may be name of cluster or `AUTO` for cluster name made of login name and checkout directory or `CURRENT` for current cluster.

### Other scripts <a name="other_scripts">

* **`k3d_push_images.py`** Pushes images to local k3d repository. Used by `helm_install.py`, but also may be used directly.
* **`ratdb_from_test.p`** Fills `ratdb` from test database. Used by `helm_install.py`, but also may be used directly.
* **`k3d_lib.py`** Not directly runnable - contains common stuff for other scripts.