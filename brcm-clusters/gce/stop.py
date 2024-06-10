#!/usr/bin/env python3
""" Stops internal and external AFC clusters """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program

# pylint: disable=too-many-arguments

import argparse
import sys
from typing import List, Optional

from utils import cluster_filename, cluster_values, DEFAULT_EXT_CONTEXT, \
    DEFAULT_INT_CONTEXT, DEFAULT_RELEASE, DEFAULT_SOURCE_ROOT, execute, \
    helm_bin, VALUES_AFC_COMMON_YAML


def stop_cluster(int_ext: str, cluster_subdir: str, context: str,
                 release: str, source_root: Optional[str]) -> None:
    """ Uninstalls prerequisites and AFC helmcharts in internal or external
    cluster

    Arguments:
    int_ext        -- "int" or "ext"
    cluster_subdir -- Subdirectory under this script's directory containing
                      cluster-specific YAML files
    context        -- '--context' argument for target scripts
    release        -- RELEASE argument for helm_install... script
    source_root    -- Optional (nondefault) source root directory
    """
    execute(
        [helm_bin(source_root, f"helm_install_{int_ext}.py"), "--uninstall",
         "--context", context, release])
    execute(
        [helm_bin(source_root, "install_prerequisites.py"), "--uninstall",
         "--context", context,
         "--extra_cfg",
         cluster_filename(cluster_subdir, VALUES_AFC_COMMON_YAML),
         "--extra_cfg",
         cluster_filename(cluster_subdir,
                          f"install-prerequisites-cfg-{int_ext}.yaml")])


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = \
        argparse.ArgumentParser(
            description="Stops internal and external AFC clusters")
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
        "--source_root", metavar="SOURCE_ROOT_DIR",
        default=DEFAULT_SOURCE_ROOT,
        help=f"Source root directory (directory having 'helm' subdirectory. "
        f"Default is '{DEFAULT_SOURCE_ROOT}'")
    argument_parser.add_argument(
        "CLUSTER",
        choices=cluster_values(),
        help="Subdirectory of this script's directory, containing "
        "cluster-specific YAML files")

    if not argv:
        argument_parser.print_help()
        sys.exit(1)

    args = argument_parser.parse_args(argv)

    stop_cluster(int_ext="ext", cluster_subdir=args.CLUSTER,
                 context=args.context_ext, release=args.release,
                 source_root=args.source_root)
    stop_cluster(int_ext="int", cluster_subdir=args.CLUSTER,
                 context=args.context_int, release=args.release,
                 source_root=args.source_root)


if __name__ == "__main__":
    main(sys.argv[1:])
