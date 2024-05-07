#!/usr/bin/env python3
""" Preparation of cluster to AFC installation """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=too-many-branches, invalid-name, too-many-locals
# pylint: disable=too-many-statements

import argparse
import datetime
import os
import re
import sys
import time
from typing import cast, List, Optional

from utils import Duration, error_if, execute, expand_filename, filter_args, \
    parse_kubecontext, SCRIPTS_DIR, yaml_load

# Environment variable holding config file name
CONFIG_ENV = "CLUSTER_PREPARE_CONFIG"

DEFAULT_CONFIG = os.path.splitext(__file__)[0] + ".yaml"


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = \
        argparse.ArgumentParser(
            description="Prepares cluster to AFC installation",
            formatter_class=argparse.RawDescriptionHelpFormatter)
    argument_parser.add_argument(
        "--cfg", metavar="SCRIPT_CONFIG_FILE",
        help=f"Config file for this script. May also be specified with "
        f"`{CONFIG_ENV}' environment variable. Default is `{DEFAULT_CONFIG}'")
    argument_parser.add_argument(
        "--context", metavar="[KUBECONFIG_FILE][:CONTEXT]",
        help="Kubeconfig file name and/or context name. Currents are used if "
        "unspecified")
    argument_parser.add_argument(
        "--uninstall", action="store_true",
        help="Uninstalls everything this script normally installs")

    args = argument_parser.parse_args(argv)

    # Reading script's config file
    cfg = \
        yaml_load(
            expand_filename(
                args.cfg or os.environ.get(CONFIG_ENV) or DEFAULT_CONFIG))
    cluster_context = parse_kubecontext(args.context)
    install_items = cfg.get("install", [])
    for item_idx in \
            range(
                (len(install_items) - 1) if args.uninstall else 0,
                -1 if args.uninstall else len(install_items),
                -1 if args.uninstall else 1):
        install_item = install_items[item_idx]
        error_if("name" not in install_item,
                 f"'name' not specified for install item #{item_idx + 1}'")
        print(f">>>{'Uninstalling' if args.uninstall else 'Installing'}: "
              f"{install_item['name']}")
        uninstall_wait = Duration(install_item.get("wait"))
        namespace: Optional[str] = install_item.get("namespace")
        new_namespace = \
            namespace and \
            (not any(install_items[idx].get("namespace") == namespace
                     for idx in range(item_idx)))
        if not args.uninstall:
            if new_namespace:
                assert namespace is not None
                execute(
                    ["kubectl", "create", "namespace", namespace] +
                    cluster_context.kubectl_args())
            for cmd in install_item.get("cmd", []):
                execute(cmd.lstrip("-"), fail_on_error=not cmd.startswith("-"))
        manifest = install_item.get("manifest")
        if manifest:
            if "://" not in manifest:
                manifest = expand_filename(manifest, root=SCRIPTS_DIR)
            execute(
                ["kubectl", "delete" if args.uninstall else "create",
                 "-f", manifest] +
                cluster_context.kubectl_args(namespace=namespace) +
                uninstall_wait.kubectl_timeout())
        elif "helm_chart" in install_item:
            helmchart: str = install_item["helm_chart"]
            assert helmchart is not None
            if not args.uninstall:
                if "helm_repo" in install_item:
                    m = re.match("^(.+?)/", helmchart)
                    error_if(not m,
                             f"fNo repository part found in helmchart name "
                             f"'{helmchart}'")
                    assert m is not None
                    execute(["helm", "repo", "add", m.group(1),
                             install_item["helm_repo"]])
                    execute(["helm", "repo", "update"])
                else:
                    helmchart = expand_filename(helmchart, root=SCRIPTS_DIR)
            error_if("helm_release" not in install_item,
                     f"'helm_release' should be specified for install item "
                     f"'{install_item['name']}'")
            helm_args = ["helm", "uninstall" if args.uninstall else "install",
                         install_item["helm_release"]] + \
                        cluster_context.helm_args(namespace=namespace)
            if not args.uninstall:
                helm_args += \
                    [helmchart] + \
                    filter_args("--version", install_item.get("helm_version"))
                for helm_values in install_item.get("helm_values", []):
                    helm_args += \
                        ["--values",
                         expand_filename(helm_values, root=SCRIPTS_DIR)]
                for helm_setting in install_item.get("helm_settings", []):
                    helm_args += ["--set", helm_setting]
                helm_args += install_item.get("args", []) + \
                    Duration(install_item.get("helm_wait")).helm_timeout()
            execute(helm_args)
            if args.uninstall and uninstall_wait:
                duration = uninstall_wait.seconds()
                assert duration is not None
                start = datetime.datetime.now()
                time.sleep(1)
                while True:
                    if "Terminating" not in \
                        cast(
                            str,
                            execute(
                                ["kubectl", "get", "all"] +
                                cluster_context.kubectl_args(
                                    namespace=namespace),
                                return_output=True)):
                        break
                    error_if(
                        (datetime.datetime.now() - start).total_seconds() >
                        duration,
                        f"Uninstall timeout expired for "
                        f"'{install_item['name']}'")
        else:
            error_if(not new_namespace,
                     f"Installation step '{install_item['name']}' appears to "
                     f"do nothing")
        if args.uninstall and new_namespace:
            assert namespace is not None
            execute(
                ["kubectl", "delete", "namespace", namespace] +
                cluster_context.kubectl_args() +
                uninstall_wait.kubectl_timeout())


if __name__ == "__main__":
    main(sys.argv[1:])
