#!/usr/bin/env python3
""" Pushes images to artifactory """
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

PUSH_REGISTRY = "wcc-afc-docker-dev-local.usw2.packages.broadcom.com"
PULL_REGISTRY = "wcc-afc-docker-virtual.usw2.packages.broadcom.com"

DEFAULT_TAG = "AUTO"

DEFAULT_SOURCE_ROOT = \
    os.path.normpath(
        os.path.abspath(os.path.join(os.path.dirname(__file__), "../..")))


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
        "--tag", metavar="IMAGE_TAG", default=DEFAULT_TAG,
        help=f"Tag of images to push to artifactory, 'AUTO' is a tag made of "
        f"login name and source root directory. Default is '{DEFAULT_TAG}'")
    argument_parser.add_argument(
        "--build", action="store_true",
        help="Build images vefore push")
    argument_parser.add_argument(
        "--source_root", metavar="SOURCE_ROOT_DIR",
        default=DEFAULT_SOURCE_ROOT,
        help=f"Source root directory (directory having 'helm' subdirectory. "
        f"Default is '{DEFAULT_SOURCE_ROOT}'")

    args = argument_parser.parse_args(argv)
    bin_dir = os.path.join(args.source_root, "helm/bin")

    print("""\
Trying to login to artifactory. If it will require password - go to okta, then
JFrog->Project WCC-AFC->Welcome->Edit Profile->Generate an Identity Token
This token is a password. Save it for future, as it will not be shown again""")
    execute(["docker", "login", PUSH_REGISTRY])
    execute(
        [os.path.join(bin_dir, "push_images.py"),
         "--registry", PULL_REGISTRY, "--push_registry", PUSH_REGISTRY] +
        (["--build"] if args.build else []) + [args.tag])


if __name__ == "__main__":
    main(sys.argv[1:])
