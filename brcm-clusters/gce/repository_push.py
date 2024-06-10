#!/usr/bin/env python3
""" Pushes images to repository """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program

# pylint: disable=invalid-name

import argparse
import sys
from typing import List, NamedTuple, Optional

from utils import cluster_values, DEFAULT_SOURCE_ROOT, error, error_if, \
    execute, helm_bin, optional_args, values_afc_common

DEFAULT_TAG = "AUTO"


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = \
        argparse.ArgumentParser(
            description="Pushes images to repository (e.g. Artifactory)")
    argument_parser.add_argument(
        "--tag", metavar="IMAGE_TAG", default=DEFAULT_TAG,
        help=f"Tag of images to push to repository, 'AUTO' is a tag made of "
        f"login name and source root directory. Default is '{DEFAULT_TAG}'")
    argument_parser.add_argument(
        "--build", action="store_true",
        help="Build images before push")
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

    RegInfo = \
        NamedTuple("RegInfo", [("pull", Optional[str]),
                               ("push", Optional[str])])
    reg_info: Optional[RegInfo] = None
    for ir in values_afc_common(args.CLUSTER).get("imageRepositories",
                                                  {}).values():
        ri = RegInfo(pull=ir.get("path"), push=ir.get("pushPath"))
        if reg_info is None:
            reg_info = ri
        elif reg_info != ri:
            error("values.yaml of internal AFC helmchart contain different "
                  "image repository information for different image types. "
                  "Images should be pushed there by some other means")
    error_if((reg_info is None) or (reg_info.pull is None),
             "Image repository information not found in values.yaml of "
             "internal AFC helmchart")
    assert reg_info is not None
    assert reg_info.pull is not None

    execute(["docker", "login", reg_info.push or reg_info.pull])
    execute(
        [helm_bin(args.source_root, "push_images.py"),
         "--registry", reg_info.pull] +
        optional_args(
            [("--push_registry", reg_info.push, reg_info.push),
             ("--build", None, args.build)]) +
        [args.tag])


if __name__ == "__main__":
    main(sys.argv[1:])
