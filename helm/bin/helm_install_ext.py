#!/usr/bin/env python3
""" Wrapper around 'helm install' of AFC server's external cluster """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program

import argparse
import os
import sys
from typing import List, Optional

from utils import ArgumentParserFromFile, auto_name, AUTO_NAME, execute, \
    EXT_HELM_REL_DIR, parse_json_output, parse_kubecontext, \
    print_helm_values, ROOT_DIR, set_silent_execute

# values.yaml path to default image tag
DEFAULT_IMAGE_TAG_PATH = "components.default.imageTag"


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = \
        ArgumentParserFromFile(
            description="Starts (external) AFC server from helmcharts. "
            "Command line parameters may also be passed via file (prefixed "
            "with '@')",
            formatter_class=argparse.RawDescriptionHelpFormatter,
            fromfile_prefix_chars="@")
    argument_parser.add_argument(
        "--uninstall", action="store_true",
        help="Stop (uninstall) AFC server if it is running")
    argument_parser.add_argument(
        "--tag", metavar="TAG",
        help="Tag of images to use. 'AUTO' means construct from username and "
        "checkout directory. If omitted then Chart.yaml's 'appVersion' "
        "is used as tag and images are loaded from remote repositories")
    argument_parser.add_argument(
        "--context", metavar="[KUBECONFIG_FILE][:CONTEXT]",
        help="Kubeconfig file name and/or context name. Context name may "
        "include wildcards (e.g. ':*.int'). Current are used if unspecified")
    argument_parser.add_argument(
        "--http", action="store_true",
        help="Enable HTTP use (default is HTTPS only)")
    argument_parser.add_argument(
        "--mtls", action="store_true",
        help="Enforce mTLS operation (client certificat checking)")
    argument_parser.add_argument(
        "--access_log", action="store_true",
        help="Enables dispatcher access log")
    argument_parser.add_argument(
        "--wait", metavar="TIMEunit",
        help="Wait for completion for up to given timeout, specified in "
        "kubectl style (e.g. 1s, 5m30s, etc.)")
    argument_parser.add_argument(
        "--upgrade", action="store_true",
        help="If helmchart is already running - do 'helm upgrade' (rolling "
        "update that preserves AFC operation continuity) and ignore "
        "'--preload_ratdb'. Default is to uninstall and (completely stop AFC) "
        "reinstall")
    argument_parser.add_argument(
        "--values", metavar="VALUES_YAML_FILE", action="append", default=[],
        help="Additional values.yaml file. If directory not specified - "
        "helmchart directory assumed. This parameter may be specified several "
        "times")
    argument_parser.add_argument(
        "--set", metavar="VA.RI.AB.LE=VALUE", action="append", default=[],
        help="Additional value setting (overrides some vaslues.yaml variable)")
    argument_parser.add_argument(
        "--print_values", choices=["yaml", "json"],
        help="Print consolidated values in given format and exit")
    argument_parser.add_argument(
        "RELEASE",
        help="Helm release name. 'AUTO' means construct from username and "
        "checkout directory")

    if not argv:
        argument_parser.print_help()
        sys.exit(1)

    args = argument_parser.parse_args(argv)

    set_silent_execute(args.print_values is not None)

    tag: Optional[str] = \
        auto_name(kabob=False) if args.tag == AUTO_NAME else args.tag
    release = auto_name(kabob=True) if args.RELEASE == AUTO_NAME \
        else args.RELEASE

    execute(["helm", "dependency", "update", EXT_HELM_REL_DIR],
            cwd=ROOT_DIR)

    # Create installation context arguments
    cluster_context = parse_kubecontext(args.context)

    # If release currently running?
    was_running = \
        any(helm_info["name"] == release
            for helm_info in
            parse_json_output(["helm", "list", "-o", "json"] +
                              cluster_context.helm_args()))

    # If release currently running and no upgrade - uninstall it first
    if was_running and (args.uninstall or (not args.upgrade)):
        execute(["helm", "uninstall", release] + cluster_context.helm_args())

    if args.uninstall:
        return

    # Preparing arguments...
    install_args = ["helm"] + \
        (["upgrade", "--install"] if args.upgrade else ["install"]) + \
        [release, os.path.join(ROOT_DIR, EXT_HELM_REL_DIR)] + \
        cluster_context.helm_args()
    # ... assumed values files ...
    for cond, filename in \
            [(args.http, "values-http.yaml"),
             (args.mtls, "values-mtls.yaml"),
             (args.access_log, "values-access_log.yaml")]:
        if not cond:
            continue
        assert isinstance(filename, str)
        install_args += ["--values",
                         os.path.abspath(os.path.join(ROOT_DIR,
                                                      EXT_HELM_REL_DIR,
                                                      filename))]
    # ... assumed settings ...
    if tag:
        install_args += ["--set", f"{DEFAULT_IMAGE_TAG_PATH}={tag}"]

    # ... specified values ...
    for filename in args.values:
        assert isinstance(filename, str)
        if not os.path.dirname(filename):
            filename = os.path.join(ROOT_DIR, EXT_HELM_REL_DIR, filename)
        install_args += ["--values", os.path.abspath(filename)]
    # ... specified settings ...
    install_args += sum((["--set", setting] for setting in args.set), [])

    # ... timeout
    if args.wait:
        install_args += ["--wait", "--timeout", args.wait]

    if args.print_values:
        print_helm_values(helm_args=install_args,
                          print_format=args.print_values)
        return

    # Executing helm install
    execute(install_args)


if __name__ == "__main__":
    main(sys.argv[1:])
