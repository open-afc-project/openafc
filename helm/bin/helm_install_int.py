#!/usr/bin/env python3
""" Wrapper around 'helm install' of AFC server's internal cluster """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program

# pylint: disable=too-many-locals, too-many-branches, too-many-statements
# pylint: disable=too-many-nested-blocks, invalid-name, too-many-arguments

import argparse
import base64
import glob
import multiprocessing
import os
import re
import shutil
import sys
import tempfile
from typing import Any, cast, Dict, List, Optional, Set
import yaml

from utils import ArgumentParserFromFile, auto_name, AUTO_NAME, \
    ClusterContext, error, error_if, execute, filter_args, \
    get_helm_values, get_known_nodeports, INT_HELM_REL_DIR, K3D_PREFIX, \
    parse_json_output, parse_k3d_reg, parse_kubecontext, print_helm_values, \
    ROOT_DIR, SCRIPTS_DIR, set_silent_execute, unused_argument, yaml_load, \
    yaml_loads

EPILOG = """\
- Recommended k3d invocation:
    $ helm/helm_install_int.py --build --tag AUTO --no_secrets --http \\
        --wait 5m --preload_ratdb AUTO

- Simplest case - load images from remote repository to current cluster (that
  has secrets loaded), expose AFC http/https:
    $ helm/helm_install.py AUTO
- Build images with given tag, no secrets, enable http, enable access to ratdb
  (also should be enabled on cluster creation), prefill ratdb from tests:
    $ helm/bin/helm_install_int.py AUTO --tag AUTO --build --no_secrets \\
        --http --expose ratdb --wait 5m --preload_ratdb
"""

# Environment variable for command line arguments
ARG_ENV_VAR = "AFC_HELM_INSTALL_ARGS"

DEFAULT_SECRET_DIR = "tools/secrets/templates"

# values.yaml path to engines per worker environment variable
ENGINES_PER_WORKER_PATH = "components.worker.env.AFC_WORKER_CELERY_CONCURRENCY"

# values.yaml path to  maximum number of workers
MAX_WORKERS_PATH = "components.worker.hpa.maxReplicas"

# values.yaml path to default image tag
DEFAULT_IMAGE_TAG_PATH = "components.default.imageTag"

# values.yaml path to image registry key
IMAGE_REGISTRY_KEY_PATH = "components.default.imageRepositoryKeyOverride"

# Parameterized values.yaml path to image registry path
CUSTOM_IMAGE_REGISTRY_PATH_PATH = "imageRepositories.{registry}.path"

# Parameterized values.yaml path to component's service type
SERVICE_TYPE_PATH = "components.{component}.service.type"

# Script for pushing imeges
PUSH_IMAGES_SCRIPT = os.path.join(SCRIPTS_DIR, "push_images.py")


class ClusterHandler:
    """ Base class for cluster-type-specific stuff. Base handles generic
    clusters (including GCP)

    Public attributes
    cluster_context -- ClusterContext object

    Protected attributes
    _args            -- Parsed command line arguments
    _known_nodeports -- dictionary of NodeportInfo objects indexed by nodeport
    _tag             -- Optional tag of images to use
    """

    def __init__(self, cluster_context: ClusterContext, args: Any,
                 tag: Optional[str]) -> None:
        """ Constructor

        Arguments:
        cluster_context -- ClusterContext object
        args            -- Parsed command line arguments
        tag             -- Tag of images to use or None
        """
        self.cluster_context = cluster_context
        self._args = args
        self._known_nodeports = get_known_nodeports(INT_HELM_REL_DIR)
        self._tag = tag

    def nodeport_components(self) -> List[str]:
        """ What components to convert to nodeport? """
        ret: Set[str] = set()
        for nodeports in (self._args.expose or []):
            for nodeport_name in nodeports.split(","):
                if nodeport_name == "":
                    continue
                error_if(nodeport_name not in self._known_nodeports,
                         f"'{nodeport_name}' is not a known nodeport name")
                ret.add(self._known_nodeports[nodeport_name].component)
        return list(ret)

    def push_images(self, build: bool) -> None:
        """ Push images to registry, used by Kubernetes (optionally building
        them before)"""
        assert self._tag is not None
        error_if(not self._args.registry,
                 "Registry to push images to not specified")
        execute([PUSH_IMAGES_SCRIPT] +
                filter_args("--registry", self._args.registry,
                            "--push_registry", self._args.push_registry,
                            "--build", build),
                self._tag)

    def value_files(self) -> List[str]:
        """ List of cluster-type-specific values.yaml override files """
        return []

    def has_registry(self) -> bool:
        """ True if image registry is known """
        return bool(self._args.registry)

    def pull_registry(self) -> Optional[str]:
        """ Name of pull image registry or None """
        return self._args.registry

    def max_workers(self, helm_args: List[str]) -> Optional[int]:
        """ Maximum number of worker replicas or None """
        unused_argument(helm_args)
        return self._args.max_workers

    @classmethod
    def create(cls, args: Any, tag: Optional[str]) -> "ClusterHandler":
        """ Create proper instance of ClusterHandler

        Arguments:
        args -- Parsed command line arguments
        tag  -- Tag of images to use or None
        """
        cluster_context = parse_kubecontext(args.context)
        current_config = \
            parse_json_output(
                ["kubectl", "config", "view", "-o", "json", "--minify"] +
                cluster_context.kubectl_args())
        if current_config["contexts"][0]["context"]["cluster"].\
                startswith(K3D_PREFIX):
            return \
                ClusterHandlerK3d(
                    cluster_context=cluster_context, args=args,
                    k3d_cluster=current_config["clusters"][0]["name"][
                        len(K3D_PREFIX):],
                    tag=tag)
        return ClusterHandler(cluster_context=cluster_context, args=args,
                              tag=tag)


class ClusterHandlerK3d(ClusterHandler):
    """ Handler for k3d clusters

    Private attributes:
    _k3d_cluster   -- K3d cluster name
    _cluster_info  -- K3d cluster information dictionary
    _registry_info -- Optional k3d image registry information
    """
    def __init__(self, cluster_context: ClusterContext, args: Any,
                 tag: Optional[str], k3d_cluster: str) -> None:
        """ Constructor

        Arguments:
        cluster_context -- ClusterContext object
        args            -- Parsed command line arguments
        tag             -- Tag of images to use or None
        k3d_cluster     -- K3d cluster name
        """
        super().__init__(cluster_context=cluster_context, args=args, tag=tag)
        self._k3d_cluster = k3d_cluster
        for cluster_info in parse_json_output(["k3d", "cluster", "list",
                                               "-o", "json"]):
            if cluster_info["name"] == self._k3d_cluster:
                self._cluster_info = cluster_info
                break
        else:
            error(f"Can't find information about k3d cluster '{k3d_cluster}'. "
                  f"Maybe it is deleted or not created")
        self._registry_info = parse_k3d_reg(self._args.k3d_reg, required=False)

    def nodeport_components(self) -> List[str]:
        """ What components to convert to nodeport? """
        if self._args.expose:
            return super().nodeport_components()
        ret: Set[str] = set()
        for node_info in self._cluster_info["nodes"]:
            for nodeport_str in node_info.get("portMappings", {}):
                m = re.match(r"^(\d+)/tcp", str(nodeport_str))
                if m is None:
                    continue
                nodeport = int(m.group(1))
                for nodeport_info in self._known_nodeports.values():
                    if nodeport_info.port == nodeport:
                        ret.add(nodeport_info.component)
                        break
        return list(ret)

    def push_images(self, build: bool) -> None:
        """ Push images to registry, used by Kubernetes (optionally building
        them before)"""
        assert self._tag is not None
        if self._args.registry:
            super().push_images(build=build)
        else:
            execute([PUSH_IMAGES_SCRIPT,
                     "--k3d_reg", self._args.k3d_reg or "DEFAULT",
                     self._tag] +
                    filter_args("--build", build))

    def value_files(self) -> List[str]:
        """ List of cluster-type-specific values.yaml override files """
        return ["values-k3d.yaml"]

    def has_registry(self) -> bool:
        """ True if image registry is known """
        return bool(self._args.registry or self._registry_info)

    def pull_registry(self) -> Optional[str]:
        """ Name of pull image registry or None """
        return self._args.registry or \
            (self._registry_info.pull if self._registry_info else None)

    def max_workers(self, helm_args: List[str]) -> Optional[int]:
        """ Maximum number of worker replicas or None """
        if self._args.max_workers:
            return super().max_workers(helm_args)
        v = get_helm_values(helm_args) or {}
        for key in ENGINES_PER_WORKER_PATH.split("."):
            v = v.get(key, {})
        if isinstance(v, int):
            return multiprocessing.cpu_count() // v
        return None


def make_fake_secrets(fake_secrets_arg: Optional[str], tempdir: str) \
        -> Optional[str]:
    """ Creates fake secrets backend definition in values.yaml format

    Arguments:
    fake_secrets_arg -- --fake_secrets argument in
                          SECRETSTORE_NAME[:SECRET_DIR] form. May be None
    tempdir            -- Directory for values.yaml override file
    Returns name of created values.yaml override file, None if fake_secrets_arg
    is None
    """
    if not fake_secrets_arg:
        return None
    parts = fake_secrets_arg.split(":")
    error_if(len(parts) > 2,
             "--fake_secrets has invalid format")
    secret_store = parts[0]
    file_or_dir = parts[1] if len(parts) > 1 \
        else os.path.join(ROOT_DIR, DEFAULT_SECRET_DIR)
    files: List[str] = []
    if os.path.isfile(file_or_dir):
        files = [file_or_dir]
    elif os.path.isdir(file_or_dir):
        files = glob.glob(os.path.join(file_or_dir, "*.yaml"))
        error_if(not files, f"No YAML files found in '{file_or_dir}'")
    else:
        error("Secrets for --fake_secrets not found")
    secrets: Dict[str, str] = {}
    for file in files:
        for yaml_idx, yaml_dict in enumerate(yaml_load(filename=file,
                                                       multidoc=True)):
            if yaml_dict.get("kind") != "Secret":
                continue
            name = yaml_dict.get("metadata", {}).get("name")
            error_if(not name, f"Secret #{yaml_idx + 1} of '{file}' doesn't "
                     f"have name")
            try:
                properties: List[str] = \
                    list(yaml_dict.get("stringData", {}).values()) + \
                    [base64.b64decode(v)
                     for v in yaml_dict.get("data", {}).values()]
            except LookupError as ex:
                error(f"Secret '{name}' from file '{file}' has invalid "
                      f"structure: {ex}")
            error_if(len(properties) != 1,
                     f"Secret '{name}' from file '{file}' has invalid "
                     f"structure: exactly one property per secret is allowed")
            error_if(name in secrets,
                     f"Duplicate definition of secret '{name}'")
            secrets[name] = properties[0]
    ret = os.path.join(tempdir, "fake_secrets.yaml")
    fake_provider = \
        {"secretStores":
         {secret_store:
          {"provider":
           {"fake":
            {"data":
             [{"key": k, "value": v} for k, v in secrets.items()]}}}}}
    with open(ret, mode="w", encoding="utf-8") as f:
        yaml.dump(fake_provider, stream=f, indent=2)
    return ret


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    known_nodeports = get_known_nodeports(INT_HELM_REL_DIR)
    argument_parser = \
        ArgumentParserFromFile(
            description="Starts (internal) AFC server from helmcharts. "
            "Command line parameters may also be passed via file(s) (prefixed "
            "with '@')",
            formatter_class=argparse.RawDescriptionHelpFormatter,
            fromfile_prefix_chars="@", epilog=EPILOG)
    argument_parser.add_argument(
        "--uninstall", action="store_true",
        help="Stop (uninstall) AFC server if it is running")
    argument_parser.add_argument(
        "--tag", metavar="TAG",
        help="Tag of images to use. 'AUTO' means construct from username and "
        "checkout directory. If omitted then Chart.yaml's 'appVersion' "
        "is used as tag and images are loaded from remote repositories")
    argument_parser.add_argument(
        "--context", metavar="[KUBECONFIG_FILE][:CONTEXT]",
        help="Kubeconfig file name and/or context name. Context name may "
        "include wildcards (e.g. ':*.int'). Current are used if unspecified")
    argument_parser.add_argument(
        "--namespace", metavar="NAMESPACE",
        help="Namespace to use. Default (for current context or in general) "
        "is used if unspecified")
    argument_parser.add_argument(
        "--push", action="store_true",
        help="Push images to local registry before starting cluster. "
        "'--tag' parameter must be specified")
    argument_parser.add_argument(
        "--build", action="store_true",
        help="Build images first. '--push' implied")
    argument_parser.add_argument(
        "--no_secrets", action="store_true",
        help="Expect no secrets to be loaded")
    argument_parser.add_argument(
        "--fake_secrets", metavar="STORE[:FILE_OR_DIR]",
        help=f"Creates 'fake' secret backend for given secret store (STORE is "
        f"an index in 'secretStores' of values.yaml) from given file with "
        f"secret manifest or directory full of such files (only *.yaml files "
        f"in it are considered). Each manifest file may contain several "
        f"secrets. Each secret should contain just one key (which is "
        f"ignoreds, as fake store is propertyless). Default secret directory "
        f"is {DEFAULT_SECRET_DIR}")
    argument_parser.add_argument(
        "--http", action="store_true",
        help="Enable HTTP use (default is HTTPS only)")
    argument_parser.add_argument(
        "--mtls", action="store_true",
        help="Enforce mTLS operation (client certificat checking)")
    argument_parser.add_argument(
        "--access_log", action="store_true",
        help="Enables dispatcher access log")
    argument_parser.add_argument(
        "--expose", metavar="NODEPORT_NAME1,NODEPORT_NAME2,...",
        action="append", default=[],
        help=f"Expose to localhost (convert to nodeports) services of "
        f"components that held given port names. List of known nodeports: "
        f"{', '.join(sorted(known_nodeports.keys()))}. On k3d default is to "
        f"expose same ports as were specified on cluster creation, plus "
        f"dispatcher's http and https ports always exposed. This parameter "
        f"may be specified several times")
    argument_parser.add_argument(
        "--wait", metavar="TIMEunit",
        help="Wait for completion for up to given timeout, specified in "
        "kubectl style (e.g. 1s, 5m30s, etc.)")
    argument_parser.add_argument(
        "--preload_ratdb", action="store_true",
        help="Preload ratdb from test database using 'ratdb_from_test.py'. "
        "Requires --wait")
    argument_parser.add_argument(
        "--upgrade", action="store_true",
        help="If helmchart is already running - do 'helm upgrade' (rolling "
        "update that preserves AFC operation continuity) and ignore "
        "'--preload_ratdb'. Default is to uninstall and (completely stop AFC) "
        "reinstall")
    argument_parser.add_argument(
        "--max_workers", metavar="MAX_WORKER_PODS", type=int,
        help=f"Maximum number of worker pods to set for autoscaler. For k3d "
        f"default is number of CPUs divided by {ENGINES_PER_WORKER_PATH} (if "
        f"set). If not set or parameter value is nonpositive - does nothing")
    argument_parser.add_argument(
        "--k3d_reg", metavar="[HOST][:PORT]",
        help="K3d registry to use for k3d operation from local repository. "
        "Default is first and only k3d registry running")
    argument_parser.add_argument(
        "--registry", metavar="PULL_REGISTRY",
        help="Image registry Kubernetes will pull images from (for non-k3d "
        "clusters)")
    argument_parser.add_argument(
        "--push_registry", metavar="PUSH_REGISTRY",
        help="Image registry to push images to, if different from pull "
        "registry (for non-k3d clusters)")
    argument_parser.add_argument(
        "--values", metavar="VALUES_YAML_FILE", action="append", default=[],
        help="Additional values.yaml file. If directory not specified - "
        "helmchart directory assumed. This parameter may be specified several "
        "times")
    argument_parser.add_argument(
        "--set", metavar="VA.RI.AB.LE=VALUE", action="append", default=[],
        help="Additional value setting (overrides some vaslues.yaml variable)")
    argument_parser.add_argument(
        "--temp_dir", metavar="DIRECTORY",
        help="Use this directory as temporary file directory and do not "
        "remove it upon completion (for debugging)")
    argument_parser.add_argument(
        "--print_values", choices=["yaml", "json"],
        help="Print consolidated values in given format and exit")
    argument_parser.add_argument(
        "RELEASE",
        help="Helm release name. 'AUTO' means construct from username and "
        "checkout directory")

    if not argv:
        argument_parser.print_help()
        sys.exit(1)

    args = argument_parser.parse_args(argv)

    error_if((args.build or args.push) and (not args.tag),
             "--build and --push require --tag")
    error_if(args.preload_ratdb and (not args.wait),
             "--preload_ratdb requires --wait")

    set_silent_execute(args.print_values is not None)

    tag: Optional[str] = \
        auto_name(kabob=False) if args.tag == AUTO_NAME else args.tag
    release = auto_name(kabob=True) if args.RELEASE == AUTO_NAME \
        else args.RELEASE

    cluster_handler = ClusterHandler.create(args=args, tag=tag)
    error_if(
        (args.build or args.push) and (not cluster_handler.has_registry()),
        "Image registry required but not found/specified")

    try:
        if args.temp_dir:
            os.makedirs(args.temp_dir, exist_ok=True)
            tempdir = os.path.abspath(args.temp_dir)
        else:
            tempdir = tempfile.mkdtemp(prefix="fake_secrets_")

        execute(["helm", "dependency", "update", INT_HELM_REL_DIR],
                cwd=ROOT_DIR)

        # Create installation context arguments
        context_args = \
            cluster_handler.cluster_context.helm_args(namespace=args.namespace)

        # If release currently running?
        was_running = \
            any(helm_info["name"] == release
                for helm_info in
                parse_json_output(["helm", "list", "-o", "json"] +
                                  context_args))

        # If release currently running and no upgrade - uninstall it first
        if was_running and (args.uninstall or (not args.upgrade)):
            execute(["helm", "uninstall", release] + context_args)

        if args.uninstall:
            return

        # Build and push as needed
        if args.build or args.push:
            cluster_handler.push_images(build=args.build)

        # Preparing arguments...
        install_args = ["helm"] + \
            (["upgrade", "--install"] if args.upgrade else ["install"]) + \
            [release, os.path.join(ROOT_DIR, INT_HELM_REL_DIR)] + context_args
        # ... assumed values files ...
        for cond, filename in \
                [(True, vf) for vf in cluster_handler.value_files()] + \
                [(args.no_secrets, "values-no_secrets.yaml"),
                 (args.http, "values-http.yaml"),
                 (args.mtls, "values-mtls.yaml"),
                 (args.access_log, "values-access_log.yaml"),
                 (args.fake_secrets,
                  make_fake_secrets(fake_secrets_arg=args.fake_secrets,
                                    tempdir=tempdir))]:
            if not cond:
                continue
            assert isinstance(filename, str)
            install_args += ["--values",
                             os.path.abspath(os.path.join(ROOT_DIR,
                                                          INT_HELM_REL_DIR,
                                                          filename))]
        # ... assumed settings ...
        if tag:
            install_args += ["--set", f"{DEFAULT_IMAGE_TAG_PATH}={tag}"]
            pull_registry = cluster_handler.pull_registry()
            if pull_registry:
                install_args += \
                    ["--set", f"{IMAGE_REGISTRY_KEY_PATH}=custom_registry",
                     "--set",
                     CUSTOM_IMAGE_REGISTRY_PATH_PATH.format(
                         registry="custom_registry") + f"={pull_registry}"]
        for component in sorted(cluster_handler.nodeport_components()):
            install_args += \
                ["--set",
                 f"{SERVICE_TYPE_PATH.format(component=component)}=NodePort"]

        # ... specified values ...
        for filename in args.values:
            assert isinstance(filename, str)
            if not os.path.dirname(filename):
                filename = os.path.join(ROOT_DIR, INT_HELM_REL_DIR, filename)
            install_args += ["--values", os.path.abspath(filename)]
        # ... specified settings ...
        install_args += sum((["--set", setting] for setting in args.set), [])

        # Setting maximum number of workers to avoid computer hang
        max_workers = cluster_handler.max_workers(helm_args=install_args)
        if (max_workers or 0) > 0:
            install_args += ["--set", f"{MAX_WORKERS_PATH}={max_workers}"]

        # ... timeout
        if args.wait:
            install_args += ["--wait", "--timeout", args.wait]

        if args.print_values:
            print_helm_values(helm_args=install_args,
                              print_format=args.print_values,
                              cwd=ROOT_DIR)
            return

        # Executing helm install
        execute(install_args)

        # Preloading ratdb
        if (not (args.upgrade and was_running)) and args.preload_ratdb:
            rft_args = [os.path.join(SCRIPTS_DIR, "ratdb_from_test.py")]
            for switch, arg in [("--namespace", args.namespace),
                                ("--context", args.context)]:
                if arg:
                    rft_args += [switch, arg]
            execute(rft_args)
    finally:
        if tempdir and (not args.temp_dir):
            shutil.rmtree(tempdir, ignore_errors=True)


if __name__ == "__main__":
    main(sys.argv[1:])
