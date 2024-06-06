#!/usr/bin/env python3
""" Preparation of cluster to AFC installation """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=too-many-branches, invalid-name, too-many-locals
# pylint: disable=too-many-statements, unnecessary-ellipsis
# pylint: disable=too-few-public-methods

import abc
import argparse
import copy
import datetime
import os
import re
import sys
import time
from typing import Any, cast, Dict, List, Optional
import yaml

from utils import ArgumentParserFromFile, ClusterContext, Duration, error, \
    error_if, execute, expand_filename, filter_args, parse_json_output, \
    parse_kubecontext, SCRIPTS_DIR, yaml_load

DEFAULT_CONFIG = os.path.splitext(__file__)[0] + ".yaml"

KEY_DEFAULT_COMPONENTS = "default_install_components"
KEY_INSTALL_COMPONENTS = "install_components"
ALL_KEYS = [KEY_DEFAULT_COMPONENTS, KEY_INSTALL_COMPONENTS]


class InstallComponent:
    """ Container of install component, allowing dot-access to its attributes
    and performing {{}}-substitutuion

    Public attributes:
    name - Component name

    Private attributes:
    _cfg       -- The whole config dictionary
    _component -- Install component content as dictionary
    """
    def __init__(self, cfg: Dict[str, Any], name: str) -> None:
        """ Constructor

        Arguments:
        cfg  -- The whole config dictionary
        name -- Install component name
        """
        assert name in cfg[KEY_INSTALL_COMPONENTS]
        self._cfg = cfg
        self.name = name
        self._component = self._cfg[KEY_INSTALL_COMPONENTS][self.name]

    def __getattr__(self, attr: str) -> Any:
        """ Install component attribute retrieval """
        if attr not in self._component:
            raise AttributeError()
        return self._recursive_substitute(copy.deepcopy(self._component[attr]))

    def _recursive_substitute(self, value: Any) -> Any:
        """ Returns component's attribute value with {{}} substitution
        performed """
        if isinstance(value, list):
            return [self._recursive_substitute(v) for v in value]
        if isinstance(value, dict):
            return {k: self._recursive_substitute(v) for k, v in value.items()}
        if isinstance(value, str):
            ret = ""
            prev_end = 0
            for m in re.finditer(r"\{\{([a-zA-Z0-9_\-.]+)\}\}", value):
                ret += value[prev_end: m.start()]
                prev_end = m.end()
                target = self._cfg
                for attr in m.group(1).split("."):
                    if isinstance(target, dict):
                        error_if(
                            attr not in target,
                            f"Invalid item reference '{m.group(0)}' in "
                            f"config: attribute '{attr}' doesn't point to "
                            f"existing item")
                        target = target[attr]
                    elif isinstance(target, list):
                        error_if(
                            not re.match(r"^\d+$", attr),
                            f"Invalid item reference '{m.group(0)}' in "
                            f"config: '{attr}' is not a valid list index")
                        idx = int(attr)
                        error_if(
                            not (0 <= idx < len(target)),
                            f"Invalid item reference '{m.group(0)}' in "
                            f"config: index '{idx}' is out of range")
                        target = target[idx]
                    else:
                        error(
                            f"Invalid item reference '{m.group(0)}' in "
                            f"config: '{attr}' addresses nothing")
                ret += str(target)
            ret += value[prev_end: len(value)]
            return ret
        return value


class InstallHandler(abc.ABC):
    """ Base class for component installer/uninstaller

    Protected attributes:
    _component        -- InstallComponent object
    _is_new_namespace -- True if namespace present and not appeared in previous
                         items
    _cluster_context  -- ClusterContext object
    _namespace        -- None or namespace used in this item
    """
    def __init__(self, component: InstallComponent, is_new_namespace: bool,
                 cluster_context: ClusterContext) -> None:
        """ Constructor

        Arguments:
        component        -- InstallComponent object
        is_new_namespace -- True if namespace present and not appeared in
                            previously installed components
        cluster_context  -- ClusterContext object
        """
        self._component = component
        self._is_new_namespace = is_new_namespace
        self._cluster_context = cluster_context
        self._namespace: Optional[str] = \
            getattr(self._component, "namespace", None)

    def install(self) -> None:
        """ Performs installation. Base implementation contains common part """
        print(f">>> Installing: {self._component.name}")
        if self._component_installed():
            print("Component already installed, skipping installation")
            return
        for cmd in getattr(self._component, "preinstall", []):
            execute(cmd.lstrip("-"), fail_on_error=not cmd.startswith("-"))
        if self._is_new_namespace and (not self._namespace_exists()):
            assert isinstance(self._namespace, str)
            execute(["kubectl", "create", "namespace", self._namespace] +
                    self._cluster_context.kubectl_args())
        self._install_impl()
        for cmd in getattr(self._component, "postinstall", []):
            execute(cmd.lstrip("-"), fail_on_error=not cmd.startswith("-"))

    def uninstall(self) -> None:
        """ Perform uninstallation """
        print(f">>> Uninstalling: {self._component.name}")
        self._uninstall_impl()
        if self._is_new_namespace and self._namespace_exists():
            assert isinstance(self._namespace, str)
            execute(["kubectl", "delete", "namespace",
                     self._component.namespace] +
                    self._cluster_context.kubectl_args() +
                    self._kubectl_uninstall_wait_args())

    def _namespace_exists(self) -> bool:
        """ True if this item's namespace exists """
        return bool(self._namespace) and \
            any(item["metadata"]["name"] == self._namespace
                for item in parse_json_output(
                    ["kubectl", "get", "namespaces", "-o", "json"] +
                    self._cluster_context.kubectl_args())["items"])

    def _kubectl_uninstall_wait_args(self) -> List[str]:
        """ List of wait arguments for manifest uninstallation """
        return Duration(getattr(self._component, "uninstall_wait", None)).\
            kubectl_timeout()

    def _component_installed(self) -> bool:
        """ True if component known to be installed """
        return False

    @abc.abstractmethod
    def _install_impl(self) -> None:
        """ Class-specific installation operations """
        ...

    @abc.abstractmethod
    def _uninstall_impl(self) -> None:
        """ Class-specific uninstallation operations """
        ...

    @classmethod
    def create(cls, component: InstallComponent, is_new_namespace: bool,
               cluster_context: ClusterContext) -> "InstallHandler":
        """ InstallHandler factory

        Arguments:
        component        -- InstallComponent object
        is_new_namespace -- True if namespace present and not appeared in
                            previously installed components
        cluster_context  -- ClusterContext object
        """
        if hasattr(component, "manifest"):
            error_if(hasattr(component, "helm_chart"),
                     f"Install item '{component.name}' has both 'manifest' "
                     f"and 'helm_chart' properties defined, this is not "
                     f"supported")
            return \
                ManifestInstallHandler(
                    component=component, is_new_namespace=is_new_namespace,
                    cluster_context=cluster_context)
        if hasattr(component, "helm_chart"):
            return \
                HelmInstallHandler(
                    component=component, is_new_namespace=is_new_namespace,
                    cluster_context=cluster_context)
        return \
            DummyInstallHandler(
                component=component, is_new_namespace=is_new_namespace,
                cluster_context=cluster_context)


class ManifestInstallHandler(InstallHandler):
    """ Installs/uninstalls manifest component """

    def _install_impl(self) -> None:
        """ Class-specific installation operations """
        execute(
            ["kubectl", "create", "-f", self._manifest_name()] +
            self._cluster_context.kubectl_args(namespace=self._namespace))

    def _uninstall_impl(self) -> None:
        """ Class-specific uninstallation operations """
        execute(
            ["kubectl", "delete", "-f", self._manifest_name(),
             "--ignore-not-found"] +
            self._cluster_context.kubectl_args(namespace=self._namespace) +
            self._kubectl_uninstall_wait_args())

    def _component_installed(self) -> bool:
        """ True if component known to be installed """
        if not hasattr(self._component, "has_service"):
            return False
        return \
            cast(bool,
                 execute(
                     ["kubectl", "get", "service",
                      self._component.has_service] +
                     self._cluster_context.kubectl_args(
                         namespace=self._namespace),
                     fail_on_error=False, hide_stderr=True))

    def _manifest_name(self) -> str:
        return self._component.manifest if "://" in self._component.manifest \
            else expand_filename(self._component.manifest, root=SCRIPTS_DIR)


class HelmInstallHandler(InstallHandler):
    """ Installs/uninstalls helm install item """

    def _install_impl(self) -> None:
        """ Class-specific installation operations """
        error_if(not hasattr(self._component, "helm_release"),
                 f"'helm_release' should be specified for component "
                 f"'{self._component.name}'")
        helmchart = self._component.helm_chart
        if hasattr(self._component, "helm_repo"):
            m = re.match("^(.+?)/", helmchart)
            error_if(
                not m,
                f"No repository part found in helmchart name'{helmchart}'")
            assert m is not None
            execute(["helm", "repo", "add", m.group(1),
                     self._component.helm_repo])
            execute(["helm", "repo", "update"])
        else:
            helmchart = expand_filename(helmchart, root=SCRIPTS_DIR)
        helm_args = \
            ["helm", "upgrade", "--install", self._component.helm_release,
             helmchart] + \
            filter_args("--version",
                        getattr(self._component, "helm_version", None)) + \
            self._cluster_context.helm_args(namespace=self._namespace)
        for helm_values in getattr(self._component, "helm_values", []):
            helm_args += \
                ["--values", expand_filename(helm_values, root=SCRIPTS_DIR)]
        for helm_setting in getattr(self._component, "helm_settings", []):
            helm_args += ["--set", helm_setting]
        helm_args += getattr(self._component, "args", []) + \
            Duration(getattr(self._component, "helm_wait", None)).\
            helm_timeout()
        execute(helm_args)

    def _uninstall_impl(self) -> None:
        """ Class-specific uninstallation operations """
        error_if(not hasattr(self._component, "helm_release"),
                 f"'helm_release' should be specified for component "
                 f"'{self._component.name}'")
        if any(item.get("name") == self._component.helm_release
                for item in
                parse_json_output(
                    ["helm", "list", "-o", "json"] +
                    self._cluster_context.helm_args(
                        namespace=self._namespace))):
            helm_args = \
                ["helm", "uninstall", self._component.helm_release] + \
                self._cluster_context.helm_args(namespace=self._namespace)
            execute(helm_args)
            uninstall_wait = \
                Duration(getattr(self._component, "uninstall_wait", None))
            if uninstall_wait:
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
                                self._cluster_context.kubectl_args(
                                    namespace=self._namespace),
                                return_output=True)):
                        break
                    error_if(
                        (datetime.datetime.now() - start).total_seconds() >
                        duration,
                        f"Uninstall timeout expired for component "
                        f"'{self._component.name}'")
                    time.sleep(5)

    # def _component_installed(self) -> bool:
    #     """ True if component known to be installed """
    #     installed_charts = \
    #         parse_json_output(
    #             ["helm", "list", "-o", "json"] +
    #             self._cluster_context.helm_args(namespace=self._namespace))
    #     return any(chartinfo.get("name") == self._component.helm_release
    #                for chartinfo in installed_charts)


class DummyInstallHandler(InstallHandler):
    """ Installs/uninstalls dummy (neither of the above) component """

    def _install_impl(self) -> None:
        """ Class-specific installation operations """
        pass

    def _uninstall_impl(self) -> None:
        """ Class-specific uninstallation operations """
        pass


def recursive_merge(base: Any, override: Any) -> Any:
    """ Recursive merge

    Arguments:
    base     -- Base item (being overridden if not merged)
    override -- Overriding value
    Returns overriding value in most cases. However if both arguments are
    dictionaries - returns merged dictionary (common elements are recursively
    merged), if lists - returns concatenation
    """
    if isinstance(override, dict):
        if not isinstance(base, dict):
            return override
        ret = {}
        for key, value in base.items():
            ret[key] = recursive_merge(value, override[key]) \
                if key in override else value
        for key, value in override.items():
            if key not in base:
                ret[key] = value
        return ret
    if isinstance(override, list):
        if not isinstance(base, list):
            return override
        return base + override
    return override


def get_cfg(argv: List[str]) -> Dict[str, Any]:
    """ Retrieving config dictionary from command line parameters """
    argument_parser = ArgumentParserFromFile(add_help=False,
                                             fromfile_prefix_chars="@")
    argument_parser.add_argument("--cfg", action="append", default=[])
    argument_parser.add_argument("--extra_cfg", action="append", default=[])
    args = argument_parser.parse_known_args(argv)[0]
    ret: Dict[str, Any] = {}
    if not args.cfg:
        ret = yaml_load(DEFAULT_CONFIG)
    for cfg_file in (args.cfg + args.extra_cfg):
        cfg_file = expand_filename(cfg_file)
        error_if(not os.path.isfile(cfg_file),
                 f"Config file '{cfg_file}' not found")
        ret = recursive_merge(ret, yaml_load(cfg_file))
    return ret


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    cfg = get_cfg(argv)

    argument_parser = \
        ArgumentParserFromFile(
            description="Install AFC Prerequisites in cluster. Command line "
            "parameters may also be passed via file(s) (prefixed with '@')",
            formatter_class=argparse.RawDescriptionHelpFormatter,
            fromfile_prefix_chars="@")
    argument_parser.add_argument(
        "--cfg", metavar="SCRIPT_CONFIG_FILE", action="append", default=[],
        help=f"Config file for this script - may be specified several times "
        f"(they then combined in given order). If none specified - default "
        f"({DEFAULT_CONFIG}) is used")
    argument_parser.add_argument(
        "--extra_cfg", metavar="SCRIPT_CONFIG_FILE", action="append",
        default=[],
        help="Additional config file for this script - may be specified "
        "several times (they then combined in given order). Unlike --cfg does "
        "not replace default config")
    argument_parser.add_argument(
        "--context", metavar="[KUBECONFIG_FILE][:CONTEXT]",
        help="Kubeconfig file name and/or context name. Context name may "
        "include wildcards (e.g. ':*.int'). Current are used if unspecified")
    argument_parser.add_argument(
        "--uninstall", action="store_true",
        help="Uninstalls specified (or all) components. Default is install")
    argument_parser.add_argument(
        "--print_cfg", action="store_true",
        help="Print resulting combined config and exit")
    argument_parser.add_argument(
        "COMPONENT", nargs="*",
        help=f"Names of components to install (valid names are: "
        f"{', '.join(cfg[KEY_INSTALL_COMPONENTS].keys())}). If this list is "
        f"empty, '{KEY_DEFAULT_COMPONENTS}' of consolidated config is used "
        f"(currently it is: "
        f"'{', '.join(cfg.get(KEY_DEFAULT_COMPONENTS, []))}'). If still empty "
        f"- on installation it is error, on uninstallation - all components "
        f"are uninstalled")

    if not argv:
        argument_parser.print_help()
        sys.exit(1)

    args = argument_parser.parse_args(argv)

    if args.print_cfg:
        print(
            yaml.dump(
                {k: v for k, v in cfg.items() if k in ALL_KEYS},
                indent=2, allow_unicode=True,
                Dumper=getattr(yaml, "CDumper", yaml.Dumper)))
        return

    # Creating install list
    error_if(not (args.uninstall or args.COMPONENT or
                  cfg.get(KEY_DEFAULT_COMPONENTS)),
             "Components to install should be specified")
    install_list = args.COMPONENT or cfg.get(KEY_DEFAULT_COMPONENTS) or \
        list(cfg[KEY_INSTALL_COMPONENTS].keys())
    for component in install_list:
        error_if(component not in cfg[KEY_INSTALL_COMPONENTS],
                 f"Unknown component name '{component}'")

    cluster_context = parse_kubecontext(args.context)
    for item_idx in \
            range(
                (len(install_list) - 1) if args.uninstall else 0,
                -1 if args.uninstall else len(install_list),
                -1 if args.uninstall else 1):
        component = InstallComponent(cfg=cfg, name=install_list[item_idx])
        namespace: Optional[str] = getattr(component, "namespace", None)
        install_handler = \
            InstallHandler.create(
                component=component,
                is_new_namespace=bool(namespace) and
                (not any(
                    getattr(InstallComponent(cfg=cfg, name=install_list[i]),
                            "namespace", None) == namespace
                    for i in range(item_idx))),
                cluster_context=cluster_context)
        if args.uninstall:
            install_handler.uninstall()
        else:
            install_handler.install()


if __name__ == "__main__":
    main(sys.argv[1:])
