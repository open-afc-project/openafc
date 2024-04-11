#!/usr/bin/env python3
""" Pushes docker images to k3d local registry """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program

# pylint: disable=invalid-name, too-many-locals

import argparse
import re
import socket
import sys
from typing import cast, Dict, List

from k3d_lib import error_if, execute, K3D_PREFIX, parse_k3d_reg


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = \
        argparse.ArgumentParser(
            description="Pushes docker images to k3d local registry")
    argument_parser.add_argument(
        "--k3d_reg", metavar="[name][:port]",
        help="Name and/or port of k3d local registry to push images to. "
        "Default is the first (and only) currently running registry")
    argument_parser.add_argument(
        "TAG",
        help="Tag of images to put to k3d registry")

    if not argv:
        argument_parser.print_help()
        sys.exit(1)

    args = argument_parser.parse_args(argv)

    registry = parse_k3d_reg(args.k3d_reg)
    registry_host, registry_port = registry.split(":")

    # Find out if 'push to localhost' should be used
    push_to_localhost = False
    try:
        socket.gethostbyname(registry_host)
    except OSError:
        push_to_localhost = True

    # Do the deed
    pushed_image_to_id: Dict[str, str] = {}
    for imgdef in cast(str, execute(["docker", "image", "list"],
                                    return_output=True)).splitlines():
        m = re.match(r"^(\S+)\s+(\S+)\s+([0-9a-f]+)", imgdef)
        if not m:
            continue
        assert m is not None
        image_full_name = m.group(1)
        image_tag = m.group(2)
        image_id = m.group(3)
        m = re.match(r"^(.*?)([^/]+)$", image_full_name)
        error_if(not m,
                 f"Unsupported structure of image name: '{image_full_name}'")
        assert m is not None
        image_repo = m.group(1)
        image_base_name = m.group(2)
        if (image_tag != args.TAG) or \
                any(image_repo.startswith(prefix)
                    for prefix in (K3D_PREFIX, "localhost")):
            continue
        pushed_id = pushed_image_to_id.get(image_base_name)
        if pushed_id:
            error_if(
                pushed_id != image_id,
                f"More than one different nonlocal versions of "
                f"'{image_base_name}:{image_tag}' found in docker repository")
            continue
        execute(
            ["docker", "image", "tag", image_id,
             f"{registry_host}:{registry_port}/{image_base_name}:{args.TAG}"])
        if push_to_localhost:
            execute(
                ["docker", "image", "tag", image_id,
                 f"localhost:{registry_port}/{image_base_name}:{args.TAG}"])
        execute(["docker", "image", "push",
                 f"{'localhost' if push_to_localhost else registry_host}:"
                 f"{registry_port}/{image_base_name}:{args.TAG}"])
        pushed_image_to_id[image_base_name] = image_id
    error_if(not pushed_image_to_id, f"No images with tag `{args.TAG}' found")


if __name__ == "__main__":
    main(sys.argv[1:])
