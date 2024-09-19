#!/usr/bin/env python3
""" Pushes docker images to registry """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program

# pylint: disable=invalid-name, too-many-locals, too-many-statements
# pylint: disable=too-many-branches

import argparse
import datetime
import os
import re
import sys
from typing import cast, Dict, List, NamedTuple, Set

from utils import auto_name, AUTO_NAME, error, error_if, execute, \
    ImageRegistryInfo, parse_json_output, parse_k3d_reg, ROOT_DIR


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = \
        argparse.ArgumentParser(
            description="Pushes docker images to k3d local registry")
    argument_parser.add_argument(
        "--k3d_reg", metavar="[NAME][:PORT] or DEFAULT",
        help="Push images to k3d image registry - either explicitly specified "
        "or the default (default is the only k3d registry, running locally)")
    argument_parser.add_argument(
        "--registry", metavar="PULL_REGISTRY",
        help="Image registry Kubernetes will pull images from")
    argument_parser.add_argument(
        "--push_registry", metavar="PUSH_REGISTRY",
        help="Image registry to push images to (may be - and strangely "
        "usually is - different from pull registry)")
    argument_parser.add_argument(
        "--build", action="store_true",
        help="Build images before push")
    argument_parser.add_argument(
        "TAG",
        help="Tag of images to put to registry. AUTO to make tag from login "
        "name and source root directory")

    if not argv:
        argument_parser.print_help()
        sys.exit(1)

    args = argument_parser.parse_args(argv)

    if args.k3d_reg:
        error_if(args.registry,
                 "--k3d_reg and --registry are mutually exclusive")
        reg_info = parse_k3d_reg(args.k3d_reg)
        assert reg_info is not None
    elif args.registry:
        reg_info = \
            ImageRegistryInfo(pull=args.registry,
                              push=args.push_registry or args.pull_registry)
    else:
        error("target registry not specified")

    args_tag = auto_name(kabob=False) if args.TAG == AUTO_NAME else args.TAG

    # Build images if requested
    if args.build:
        execute([os.path.join(ROOT_DIR, "tests/regression/build_imgs.sh"),
                 ROOT_DIR, args_tag, "0"])
    # Chose latest images of given tag
    ImageInfo = NamedTuple("ImageInfo", [("image_id", str),
                                         ("created", datetime.datetime),
                                         ("known_ids", Set[str])])
    images: Dict[str, ImageInfo] = {}

    for imgdef in cast(str, execute(["docker", "image", "list"],
                                    return_output=True)).splitlines():
        m = re.match(r"^(\S+)\s+(\S+)\s+([0-9a-f]+)", imgdef)
        if not m:
            # Probably image table heading
            continue
        assert m is not None
        image_full_name = m.group(1)
        image_tag = m.group(2)
        image_id = m.group(3)
        m = re.match(r"^(.*?)([^/]+)$", image_full_name)
        error_if(not m,
                 f"Unsupported structure of image name: '{image_full_name}'")
        assert m is not None
        image_base_name = m.group(2)
        if image_tag != args_tag:
            # Other tag
            continue
        current_image_info = images.get(image_base_name)
        if current_image_info and (image_id in current_image_info.known_ids):
            # Image already found
            continue
        image_inspection = \
            parse_json_output(["docker", "image", "inspect", image_id],
                              echo=False)
        try:
            created = \
                datetime.datetime.fromisoformat(
                    image_inspection[0]["Created"])
            if current_image_info and (created <= current_image_info.created):
                # Same or fresher image already found
                continue
        except LookupError as ex:
            error(f"Unsupported format of image '{imgdef}' inspection: {ex}")
        except ValueError as ex:
            error(f"Unsuported creation tiem format of image '{imgdef}': {ex}")
        images[image_base_name] = \
            ImageInfo(
                image_id=image_id, created=created,
                known_ids={image_id} |
                (current_image_info.known_ids if current_image_info
                 else set()))
    error_if(not images,
             "No eligible images found. Maybe they are not yet built?")

    # Pushing selected images
    for image_base_name, image_info in images.items():
        execute(
            ["docker", "image", "tag", image_info.image_id,
             f"{reg_info.pull}/{image_base_name}:{args_tag}"])
        if reg_info.push != reg_info.pull:
            execute(
                ["docker", "image", "tag", image_info.image_id,
                 f"{reg_info.push}/{image_base_name}:{args_tag}"])
        execute(["docker", "image", "push",
                 f"{reg_info.push}/{image_base_name}:{args_tag}"])


if __name__ == "__main__":
    main(sys.argv[1:])
