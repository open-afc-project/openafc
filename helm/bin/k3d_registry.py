#!/usr/bin/env python3
""" Creates/deletes k3d image registry (if not yet created) """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program

import argparse
import sys
from typing import List

from utils import error_if, execute, K3D_PREFIX, parse_json_output

DEFAULT_REGISTRY_NAME = "afc-registry.localhost"


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = \
        argparse.ArgumentParser(
            description="Creates k3d image registry (if not yet created)")
    argument_parser.add_argument(
        "--delete", action="store_true",
        help="Delete image registry (default - create)")
    argument_parser.add_argument(
        "--force", action="store_true",
        help="On creation: create image registry even though some other k3d "
        "image registry already running. On deletion: delete registries even "
        "if more than one running")
    argument_parser.add_argument(
        "--registry", metavar="REGISTRY_NAME",
        help=f"Name of registry to create/delete. Default is to create "
        f"registry '{DEFAULT_REGISTRY_NAME}' and delete whatever (single!) "
        f"registry is running")

    args = argument_parser.parse_args(argv)

    registries: List[str] = \
        [reg["name"] for reg in
         parse_json_output(["k3d", "registry", "list", "-o", "json"])]
    already_running = bool(args.registry) and \
        any(registry in (args.registry, K3D_PREFIX + args.registry)
            for registry in registries)
    if args.delete:
        if args.registry:
            if already_running:
                execute(["k3d", "registry", "delete", args.registry])
            else:
                print(f"Registry '{args.registry}' is not currently running")
        elif len(registries) == 0:
            print("No registries currently running")
        else:
            error_if((len(registries) > 1) and (not args.force),
                     "More than one registy running. Specify name of registry "
                     "to delete or --force to delete all")
            for registry in registries:
                execute(["k3d", "registry", "delete", registry])
    else:
        error_if(already_running,
                 f"Registry '{args.registry}' already running")
        if args.force:
            error_if(not args.registry,
                     "--force requires explicit registry name specification")
            execute(["k3d", "registry", "create", args.registry])
        else:
            if registries:
                print("Registry is currently running")
            else:
                execute(["k3d", "registry", "create",
                         args.registry or DEFAULT_REGISTRY_NAME])


if __name__ == "__main__":
    main(sys.argv[1:])
