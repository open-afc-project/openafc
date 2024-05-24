#!/usr/bin/env python3
""" Starts dev cluster """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program

# pylint: disable=invalid-name

import argparse
import json
import os
import shlex
import subprocess
import sys
from typing import Any, List, Optional, Tuple

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
            description="Starts dev cluster on GCP")
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
        "--access_log", action="store_true",
        help="Enables dispatcher access log")
    argument_parser.add_argument(
        "--max_workers", metavar="MAX_WORKER_PODS", type=int,
        help="Maximum number of worker pods to set for autoscaler")
    argument_parser.add_argument(
        "--internal", action="store_true",
        help="Only reloads (upgrades) internal AFC helmchart")
    argument_parser.add_argument(
        "--source_root", metavar="SOURCE_ROOT_DIR",
        default=DEFAULT_SOURCE_ROOT,
        help=f"Source root directory (directory having 'helm' subdirectory. "
        f"Default is '{DEFAULT_SOURCE_ROOT}'")

    args = argument_parser.parse_args(argv)
    bin_dir = os.path.join(args.source_root, "helm/bin")
    if not args.internal:
        execute(
            [os.path.join(bin_dir, "install_prerequisites.py"),
             "--context", args.context_int,
             "--extra_cfg", os.path.join(SCRIPT_DIR, "values-afc-common.yaml"),
             "--extra_cfg", os.path.join(SCRIPT_DIR,
                                         "install_prerequisites_cfg_int.yaml"),
             "prometheus_operator", "prometheus", "prometheus_adapter",
             "external_secrets", "ingress_nginx"])
    execute(
        [os.path.join(bin_dir, "helm_install_int.py"), "--upgrade",
         "--context", args.context_int,
         "--values", os.path.join(SCRIPT_DIR, "values-afc-int.yaml"),
         "--values", os.path.join(SCRIPT_DIR, "values-afc-common.yaml")] +
        optional_args(
            [("--tag", args.tag, args.tag),
             ("--max_workers", args.max_workers, args.max_workers)]) +
        ["--wait", "5m", args.release])
    helm_ext_args = \
        [os.path.join(bin_dir, "helm_install_ext.py"), "--upgrade",
         "--context", args.context_ext,
         "--values", os.path.join(SCRIPT_DIR, "values-afc-ext.yaml"),
         "--values", os.path.join(SCRIPT_DIR, "values-afc-common.yaml")] + \
        optional_args([("--tag", args.tag, args.tag),
                       ("--http", None, args.http),
                       ("--mtls", None, args.mtls),
                       ("--access_log", None, args.access_log)]) + \
        ["--wait", "5m", args.release]

    if not args.internal:
        execute(
            [os.path.join(bin_dir, "install_prerequisites.py"),
             "--context", args.context_ext, "external_secrets"])
        execute(helm_ext_args)
    ext_values = \
        json.loads(subprocess.check_output(
            helm_ext_args + ["--print_values", "json"]))
    print("External IPs:")
    for name, ip in ext_values.get("externalIps", {}).items():
        if ip:
            print(f"  {name}: {ip}")


if __name__ == "__main__":
    main(sys.argv[1:])
