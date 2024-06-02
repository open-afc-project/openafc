#!/usr/bin/env python3
""" Starts AFC in k3d cluster """
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
from typing import Any, List, Optional, Tuple

DEFAULT_CLUSTER = "AUTO"
DEFAULT_TAG = "AUTO"
DEFAULT_RELEASE = "AUTO"
DEFAULT_EXPOSE = \
    "http,https,ratdb,rat-server,msghnd,bulk-postgres,rcache,grafana," \
    "prometheus"

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


def optional_args(args: List[Tuple[str, Optional[str], Any]]) -> List[str]:
    """ Optional subprocess arguments

    Arguments:
    args -- List of (param, value, predicate) tuples. Parameters with false
            predicates are skipped. Parameters with None value are generated
            without value
    Returns subprocess argument list for given argument descriptors
    """
    ret: List[str] = []
    for param, value, pred in args:
        if not pred:
            continue
        ret.append(param)
        if value is not None:
            ret.append(value)
    return ret


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
        "--tag", metavar="IMAGE_TAG", default=DEFAULT_TAG,
        help=f"Tag of AFC images to use. 'AUTO' means tag made of username "
        f"and source root directory. Default is '{DEFAULT_TAG}'")
    argument_parser.add_argument(
        "--release", metavar="AFC_HELM_RELEASE",
        default=DEFAULT_RELEASE,
        help=f"Helm release to use for AFC. 'AUTO' creates release name from "
        f"login name and source root directory. Default is "
        f"'{DEFAULT_RELEASE}'")
    argument_parser.add_argument(
        "--expose", metavar="SERVICES", default=DEFAULT_EXPOSE,
        help=f"Comma-separated list of services to expose via localhost "
        f"ports. Default is '{DEFAULT_EXPOSE}'")
    argument_parser.add_argument(
        "--max_workers", metavar="MAX_WORKER_PODS",
        help="Maximum number of worker pods to set for autoscaler. Default is "
        "a number that yields one AFC engine per CPU")
    argument_parser.add_argument(
        "--push", action="store_true",
        help="Push images to k3d registry before starting cluster")
    argument_parser.add_argument(
        "--build", action="store_true",
        help="Build and push images to k3d registry before starting cluster")
    argument_parser.add_argument(
        "--preload_ratdb", action="store_true",
        help="Preload ratdb from test database")
    argument_parser.add_argument(
        "--http", action="store_true",
        help="Enable HTTP use (default is HTTPS only)")
    argument_parser.add_argument(
        "--mtls", action="store_true",
        help="Enforce mTLS operation (client certificate checking)")
    argument_parser.add_argument(
        "--access_log", action="store_true",
        help="Enables dispatcher access log")
    argument_parser.add_argument(
        "--internal", action="store_true",
        help="Only reloads (upgrades) internal AFC helmchart")
    argument_parser.add_argument(
        "--source_root", metavar="SOURCE_ROOT_DIR",
        default=DEFAULT_SOURCE_ROOT,
        help=f"Source root directory (directory having 'helm' subdirectory. "
        f"Default is '{DEFAULT_SOURCE_ROOT}'")
    argument_parser.add_argument(
        "--set", metavar="VA.RI.AB.LE=VALUE", action="append", default=[],
        help="Additional setting for values.yaml of AFC helmchart")

    args = argument_parser.parse_args(argv)
    bin_dir = os.path.join(args.source_root, "helm/bin")
    if not args.internal:
        execute([os.path.join(bin_dir, "k3d_registry.py")])
        execute([os.path.join(bin_dir, "k3d_cluster_create.py"),
                 "--expose", args.expose, args.cluster])
    execute(
        [os.path.join(bin_dir, "helm_install_int.py"), "--tag", args.tag,
         "--values", os.path.join(SCRIPT_DIR, "values-afc-int.yaml"),
         "--fake_secrets",
         f"secret-store:{os.path.join(SCRIPT_DIR, 'secrets')}",
         "--wait", "5m"] +
        optional_args([("--max_workers", args.max_workers, args.max_workers),
                       ("--push", None, args.push),
                       ("--build", None, args.build),
                       ("--preload_ratdb", None, args.preload_ratdb),
                       ("--http", None, args.http),
                       ("--mtls", None, args.mtls),
                       ("--access_log", None, args.access_log)]) +
        sum([["--set", s] for s in args.set], []) +
        [args.release])
    execute([os.path.join(bin_dir, "k3d_ports.py"), "CURRENT"])


if __name__ == "__main__":
    main(sys.argv[1:])
