#!/usr/bin/env python3
""" Stops dev cluster """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program

import argparse
import os
import shlex
import subprocess
import sys
from typing import List

DEFAULT_INT_CONTEXT = ":*int"
DEFAULT_EXT_CONTEXT = ":*ext"
DEFAULT_RELEASE = "AUTO"
SCRIPT_DIR = os.path.dirname(__file__)
DEFAULT_SOURCE_ROOT = \
    os.path.normpath(os.path.abspath(os.path.join(SCRIPT_DIR, "../..")))


def execute(args: List[str]) -> None:
    """ Execute given command """
    try:
        print(f">>> {' '.join(shlex.quote(arg) for arg in args)}")
        subprocess.run(args, check=True)
    except (OSError, subprocess.CalledProcessError) as ex:
        print(f"Execution failed: {ex}", file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        sys.exit(1)


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = \
        argparse.ArgumentParser(
            description="Stops dev cluster on GCP")
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

    args = argument_parser.parse_args(argv)
    bin_dir = os.path.join(args.source_root, "helm/bin")
    execute(
        [os.path.join(bin_dir, "helm_install_ext.py"), "--uninstall",
         "--context", args.context_ext, args.release])
    execute(
        [os.path.join(bin_dir, "install_prerequisites.py"), "--uninstall",
         "--context", args.context_ext, "external_secrets"])
    execute(
        [os.path.join(bin_dir, "helm_install_int.py"), "--uninstall",
         "--context", args.context_int, args.release])
    execute(
        [os.path.join(bin_dir, "install_prerequisites.py"), "--uninstall",
         "--context", args.context_int,
         "prometheus_operator", "prometheus", "prometheus_adapter",
         "external_secrets", "ingress_nginx"])


if __name__ == "__main__":
    main(sys.argv[1:])
