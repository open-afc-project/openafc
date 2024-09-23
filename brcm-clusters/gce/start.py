#!/usr/bin/env python3
""" Starts internal and external clusters """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program

# pylint: disable=invalid-name, too-many-locals, too-many-arguments

import argparse
import os
import shlex
import sys
from typing import List, Optional

from utils import cluster_filename, cluster_values, DEFAULT_EXT_CONTEXT, \
    DEFAULT_INT_CONTEXT, DEFAULT_SOURCE_ROOT, DEFAULT_RELEASE, execute, \
    helm_bin, optional_args, values_afc_common, VALUES_AFC_COMMON_YAML

# Environment variable containing additional arguments
PARAM_ENV = "AFC_START_ARGS"


def start_cluster(int_ext: str, cluster_subdir: str, context: str,
                  release: str, source_root: Optional[str],
                  install_prerequisites: bool, install_helm: bool,
                  print_ips: bool, extra_args: List[str],
                  extra_cfg: Optional[List[str]]) -> None:
    """ Install prerequisites and AFC helmcharts in internal or external
    cluster

    Arguments:
    int_ext               -- "int" or "ext"
    cluster_subdir        -- Subdirectory under this script's directory
                             containing cluster-specific YAML files
    context               -- '--context' argument for target scripts
    release               -- RELEASE argument for helm_install... script
    source_root           -- Optional (nondefault) source root directory
    install_prerequisites -- True to install prerequisites
    install_helm          -- True to install AFC helmcharts
    print_ips             -- True to print IPs
    extra_args            -- Additional arguments for helm_install... script
    extra_cfg             -- Values for --extra_cfg parameters for
                             install_prerequisites script. Filenames without
                             directory assumed to e in 'cluxster_subdir'
    """
    if install_prerequisites:
        ec = \
            [cluster_filename(cluster_subdir, VALUES_AFC_COMMON_YAML),
             cluster_filename(cluster_subdir,
                              f"install-prerequisites-cfg-{int_ext}.yaml")] + \
            [(fn if os.path.dirname(fn)
              else cluster_filename(cluster_subdir, fn))
             for fn in (extra_cfg or [])]
        execute(
            [helm_bin(source_root, "install_prerequisites.py"),
             "--context", context,
             "--extra_cfg",
             cluster_filename(cluster_subdir, VALUES_AFC_COMMON_YAML),
             "--extra_cfg",
             cluster_filename(cluster_subdir,
                              f"install-prerequisites-cfg-{int_ext}.yaml")] +
             sum([["--extra_cfg", fn] for fn in ec], []))
    if install_helm:
        execute(
            [helm_bin(source_root, f"helm_install_{int_ext}.py"),
             "--values", cluster_filename(cluster_subdir,
                                          f"values-afc-{int_ext}.yaml"),
             "--values", cluster_filename(cluster_subdir,
                                          VALUES_AFC_COMMON_YAML),
             "--wait", "5m", release, "--context", context] +
            (extra_args or []))
    if print_ips:
        print("External IPs:")
        for name, ip in values_afc_common(cluster_subdir).get("externalIps",
                                                              {}).items():
            if ip:
                print(f"  {name}: {ip}")


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = \
        argparse.ArgumentParser(
            description=f"Starts internal and external clusters. Additional "
            f"parameters may be specified in {PARAM_ENV} environment variable")
    argument_parser.add_argument(
        "--tag", metavar="IMAGE_TAG",
        help="Tag of AFC images to use. By default Chart.yaml's 'appVersion' "
        "is used as tag")
    argument_parser.add_argument(
        "--release", metavar="AFC_HELM_RELEASE",
        default=DEFAULT_RELEASE,
        help=f"Helm release to use for AFC in inner and outer cluster. 'AUTO' "
        f"creates release name from login name and checkout directory. "
        f"Default is '{DEFAULT_RELEASE}'")
    argument_parser.add_argument(
        "--context_int", metavar="[KUBECONFIG_FILE][:CONTEXT]",
        default=DEFAULT_INT_CONTEXT,
        help=f"Kubeconfig file name and/or context name for internal AFC "
        f"cluster. Context name may include wildcards (e.g. ':*.int'). "
        f"Default is '{DEFAULT_INT_CONTEXT}'")
    argument_parser.add_argument(
        "--context_ext", metavar="[KUBECONFIG_FILE][:CONTEXT]",
        default=DEFAULT_EXT_CONTEXT,
        help=f"Kubeconfig file name and/or context name for external AFC "
        f"cluster. Context name may include wildcards (e.g. ':*.ext'). "
        f"Default is '{DEFAULT_EXT_CONTEXT}'")
    argument_parser.add_argument(
        "--http", action="store_true",
        help="Enable HTTP use (default is HTTPS only)")
    argument_parser.add_argument(
        "--mtls", action="store_true",
        help="Enforce mTLS operation (client certificate checking)")
    argument_parser.add_argument(
        "--msghnd", action="store_true",
        help="Use legacy msghnd AFC Request handler (instead of afcserver)")
    argument_parser.add_argument(
        "--devel", action="store_true",
        help="Enables development features of various containers")
    argument_parser.add_argument(
        "--max_workers", metavar="MAX_WORKER_PODS", type=int,
        help="Maximum number of worker pods to set for autoscaler")
    argument_parser.add_argument(
        "--no_prerequisites", action="store_true",
        help="Only reloads (upgrades) top-level AFC helmcharts")
    argument_parser.add_argument(
        "--source_root", metavar="SOURCE_ROOT_DIR",
        default=DEFAULT_SOURCE_ROOT,
        help=f"Source root directory (directory having 'helm' subdirectory. "
        f"Default is '{DEFAULT_SOURCE_ROOT}'")
    argument_parser.add_argument(
        "--set_int", metavar="VA.RI.AB.LE=VALUE", action="append", default=[],
        help="Additional setting for values.yaml of internal AFC helmchart")
    argument_parser.add_argument(
        "--set_ext", metavar="VA.RI.AB.LE=VALUE", action="append", default=[],
        help="Additional setting for values.yaml of external AFC helmchart")
    argument_parser.add_argument(
        "--extra_cfg_int", metavar="FILENAME", action="append",
        help="--extra_cfg for install_prerequisites.py of internal cluster. "
        "May be specified several times. If filename has no directory name - "
        "it is assumed top be in subdirectory of cluster-specific files")
    argument_parser.add_argument(
        "--extra_cfg_ext", metavar="FILENAME", action="append",
        help="--extra_cfg for install_prerequisites.py of external cluster. "
        "May be specified several times. If filename has no directory name - "
        "it is assumed top be in subdirectory of cluster-specific files")
    argument_parser.add_argument(
        "CLUSTER",
        choices=cluster_values(),
        help="Subdirectory of this script's directory, containing "
        "cluster-specific YAML files")

    if not argv:
        argument_parser.print_help()
        sys.exit(1)

    args = \
        argument_parser.parse_args(
            argv + shlex.split(os.environ.get(PARAM_ENV, "")))

    start_cluster(
        int_ext="int", cluster_subdir=args.CLUSTER,
        context=args.context_int, release=args.release,
        source_root=args.source_root,
        install_prerequisites=not args.no_prerequisites,
        install_helm=True, print_ips=False,
        extra_args=optional_args(
            [("--tag", args.tag, args.tag),
             ("--msghnd", None, args.msghnd),
             ("--max_workers", str(args.max_workers), args.max_workers),
             ("--devel", None, args.devel)]) +
        sum([["--set", s] for s in args.set_int], []),
        extra_cfg=args.extra_cfg_int)
    start_cluster(
        int_ext="ext", cluster_subdir=args.CLUSTER,
        context=args.context_ext, release=args.release,
        source_root=args.source_root,
        install_prerequisites=not args.no_prerequisites,
        install_helm=True, print_ips=True,
        extra_args=optional_args(
            [("--tag", args.tag, args.tag),
             ("--http", None, args.http),
             ("--mtls", None, args.mtls),
             ("--devel", None, args.devel)]) +
        sum([["--set", s] for s in args.set_ext], []),
        extra_cfg=args.extra_cfg_ext)


if __name__ == "__main__":
    main(sys.argv[1:])
