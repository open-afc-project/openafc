#!/usr/bin/env python3
""" Wrapper around 'helm install' of AFC (internal) server """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program

# pylint: disable=too-many-locals, too-many-branches, too-many-statements

import argparse
import enum
import os
import shlex
import sys
from typing import List, Optional, Set

from k3d_lib import auto_name, AUTO_NAME, error, error_if, execute, \
    get_known_nodeports, INT_HELM_REL_DIR, K3D_PREFIX, parse_json_output, \
    parse_k3d_reg, parse_kubecontext, ROOT_DIR, SCRIPTS_DIR

EPILOG = """\
- Simplest case - load images from remote repository to current cluster (that
  has secrets loaded), expose AFC http/https:
    $ helm/helm_install.py AUTO
- Build images with given tag, no secrets, enable http, enable access to ratdb
  (also should be enabled on cluster creation), prefill ratdb from tests:
    $ helm/bin/helm_install.py AUTO --tag AUTO --build --no_secrets --http \\
        --expose ratdb --wait 5m --preload_ratdb
"""

# Supported cluster types
ClusterType = enum.Enum("ClusterType", ["K3d"])


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    known_nodeports = get_known_nodeports(INT_HELM_REL_DIR)
    argument_parser = \
        argparse.ArgumentParser(
            description="Starts (internal) AFC server from helmcharts",
            formatter_class=argparse.RawDescriptionHelpFormatter,
            epilog=EPILOG)
    argument_parser.add_argument(
        "--tag", metavar="TAG",
        help="Tag of images to use. 'AUTO' means construct from username and "
        "checkout directory. If omitted then Chart.yaml's 'appVersion' "
        "is used as tag and images are loaded from remote repositories")
    argument_parser.add_argument(
        "--context", metavar="[KUBECONFIG_FILE][:CONTEXT]",
        help="Kubeconfig file name and/or context name. Current are used if "
        "unspecified")
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
        "--http", action="store_true",
        help="Enable HTTP use (default is HTTPS only)")
    argument_parser.add_argument(
        "--expose", metavar="NODEPORT_NAME1,NODEPORT_NAME2,...",
        action="append", default=[],
        help=f"Expose to localhost(convert to nodeports) service of "
        f"components that held given port names. On k3d dispatcher is "
        f"nodeport (exposed) anyway. List of known nodeports: "
        f"{', '.join(sorted(known_nodeports.keys()))}. For this to work on "
        f"k3d nodeports should also be exposed during cluster creation. This "
        f"parameter may be specified several times")
    argument_parser.add_argument(
        "--wait", metavar="TIMEunit",
        help="Wait for completion for up to given timeout, specified in "
        "kubectl style (e.g. 1s, 5m30s, etc.)")
    argument_parser.add_argument(
        "--preload_ratdb", action="store_true",
        help="Preload ratdb from test database using 'ratdb_from_test.py'. "
        "Requires --wait")
    argument_parser.add_argument(
        "--upgrade", action="store true",
        help="If helmchart is already running - do 'helm upgrade' (rolling "
        "update that preserves AFC operation continuity) and ignore "
        "'--preload_ratdb'. Default is to uninstall and (completely stop AFC) "
        "reinstall")
    argument_parser.add_argument(
        "--k3d_reg", metavar="[HOST][:PORT]",
        help="K3d registry to use for k3d operation from local repository. "
        "Default is first and only k3d registry running")
    argument_parser.add_argument(
        "--values", metavar="VALUES_YAML_FILE", action="append", default=[],
        help="Additional values.yaml file. If directory not specified - "
        "helmchart directory assumed. This parameter may be specified several "
        "times")
    argument_parser.add_argument(
        "--set", metavar="VA.RI.AB.LE=VALUE", action="append", default=[],
        help="Additional value setting (overrides some vaslues.yaml variable)")
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

    tag: Optional[str] = \
        auto_name(kabob=False) if args.tag == AUTO_NAME else args.tag
    release = auto_name(kabob=True) if args.RELEASE == AUTO_NAME else args.RELEASE

    # What components to convert to nodeport?
    nodeport_components: Set[str] = set()
    for nodeports in args.expose:
        for nodeport in nodeports.split(","):
            error_if(nodeport not in known_nodeports,
                     f"'{nodeport}' is not a known nodeport name")
            nodeport_components.add(known_nodeports[nodeport].component)

    kubeconfig, kubecontext = parse_kubecontext(args.context)

    current_config = \
        parse_json_output(
            ["kubectl", "config", "view", "-o", "json", "--minify"] +
            (["--kubeconfig", kubeconfig] if kubeconfig else []) +
            (["--context", kubecontext] if kubecontext else []))
    if current_config["contexts"][0]["context"]["cluster"].\
            startswith(K3D_PREFIX):
        cluster_type = ClusterType.K3d
    else:
        error("Only k3d clusters supported for now. Stay tuned!")

    k3d_registry: Optional[str] = parse_k3d_reg(args.k3d_reg) \
        if (cluster_type == ClusterType.K3d) else None

    # Create installation context arguments
    context_args: List[str] = []
    for switch, arg, in [("--kubeconfig", kubeconfig),
                         ("--kube-context", kubecontext),
                         ("--namespace", args.namespace)]:
        if arg:
            context_args += [switch, arg]

    # If release currently running - uninstall it first
    upgrade = False        
    for helm_info in \
            parse_json_output(["helm", "list", "-o", "json"] + context_args):
        if helm_info["name"] == release:
            if args.upgrade:
                upgrade = True
            else:
                execute(["helm", "uninstall", release] + context_args)
            break

    # Build and push as needed
    if args.build:
        assert tag is not None
        if cluster_type == ClusterType.K3d:
            execute([os.path.join(ROOT_DIR, "tests/regression/build_imgs.sh"),
                     ROOT_DIR, tag, "0"])
    if args.build or args.push:
        assert tag is not None
        if cluster_type == ClusterType.K3d:
            execute([os.path.join(SCRIPTS_DIR, "k3d_push_images.py"), tag] +
                    (["--k3d_reg", args.k3d_reg] if args.k3d_reg else []))

    # Preparing arguments...
    install_args = ["helm", "upgrade" if upgrade else "install", release,
                    INT_HELM_REL_DIR] + context_args
    # ... assumed values files ...
    for cond, filename in \
            [(cluster_type == ClusterType.K3d, "values-k3d.yaml"),
             (args.no_secrets, "values-no_secrets.yaml"),
             (args.http, "values-http.yaml")]:
        if not cond:
            continue
        install_args += ["--values",
                         os.path.abspath(os.path.join(ROOT_DIR,
                                                      INT_HELM_REL_DIR,
                                                      filename))]
    # ... assumed settings ...
    for cond, setting, value in \
            [(tag, "components.default.imageTag", tag),
             (tag and (cluster_type == ClusterType.K3d),
              "imageRepositories.k3d.path", k3d_registry),
             (tag and (cluster_type == ClusterType.K3d),
             "components.default.imageRepositoryKeyOverride", "k3d")]:
        if not cond:
            continue
        assert value is not None
        install_args += ["--set", f"{setting}={shlex.quote(value)}"]
    # ... nodeport holding components ...
    for component in sorted(nodeport_components):
        install_args += \
            ["--set", f"components.{component}.serviceType=NodePort"]

    # ... specified values ...
    for filename in args.values:
        if not os.path.dirname(filename):
            filename = os.path.join(ROOT_DIR, INT_HELM_REL_DIR, filename)
        install_args += ["--values", os.path.abspath(filename)]
    # ... specified settings ...
    install_args += sum((["--set", setting] for setting in args.set), [])
    # ... timeout
    if args.wait:
        install_args += ["--wait", "--timeout", args.wait]

    # Executing helm install
    execute(["helm", "dependency", "update", INT_HELM_REL_DIR], cwd=ROOT_DIR)
    execute(install_args, cwd=ROOT_DIR)

    # Preloading ratdb
    if (not upgrade) and args.preload_ratdb:
        rft_args = [os.path.join(SCRIPTS_DIR, "ratdb_from_test.py")]
        for switch, arg in [("--namespace", args.namespace),
                            ("--context", args.context)]:
            if arg:
                rft_args += [switch, arg]
        execute(rft_args)


if __name__ == "__main__":
    main(sys.argv[1:])
