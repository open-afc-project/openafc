#!/usr/bin/env python3
""" Stops AFC k3d cluster """
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

DEFAULT_CLUSTER = "AUTO"

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
            description="Starts AFC in k3d cluster")
    argument_parser.add_argument(
        "--cluster", metavar="CLUSTER_NAME", default=DEFAULT_CLUSTER,
        help=f"Cluster to start. 'AUTO' means name made of username and "
        f"source root directory. Default is '{DEFAULT_CLUSTER}'")
    argument_parser.add_argument(
        "--source_root", metavar="SOURCE_ROOT_DIR",
        default=DEFAULT_SOURCE_ROOT,
        help=f"Source root directory (directory having 'helm' subdirectory. "
        f"Default is '{DEFAULT_SOURCE_ROOT}'")

    args = argument_parser.parse_args(argv)
    bin_dir = os.path.join(args.source_root, "helm/bin")
    execute([os.path.join(bin_dir, "k3d_cluster_create.py"), "--delete",
             args.cluster])


if __name__ == "__main__":
    main(sys.argv[1:])
