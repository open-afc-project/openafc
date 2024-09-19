#!/usr/bin/env python3
""" Wrapper around 'k3d cluster create' that calls it with proper parameters
"""
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=invalid-name, too-many-locals, too-many-branches
# pylint: disable=too-many-statements, too-few-public-methods

import argparse
import os
import re
import sys
import tempfile
from typing import cast, Dict, List, NamedTuple, Optional, Set

from utils import auto_name, AUTO_NAME, error, error_if, execute, \
    expand_filename, get_known_nodeports, ImageRegistryInfo, \
    INT_HELM_REL_DIR, K3D_PREFIX, parse_json_output, parse_k3d_reg, \
    SCRIPTS_DIR, warning_if

# List of known static file locations
KNOWN_STATIC_FILE_LOCATIONS = ["/opt/afc/databases/rat_transfer"]

# List of files/directories to be in directory to qualify as static file
# directory
STATIC_FILE_EXPETED_CONTENT = ["ULS_Database", "RAS_Database"]

# List of ports names expose by default
DEFAULT_EXPOSE = ["http", "https"]

# Cluster preparation script
INSTALL_PREREQUISITES_SCRIPT = \
    os.path.join(SCRIPTS_DIR, "install_prerequisites.py")

# Components to install in cluster
INSTALL_PREREQUISITES_COMPONENTS = ["prometheus_operator", "prometheus",
                                    "prometheus_adapter", "external_secrets"]


EPILOG = """\
This script concocts 'k3d cluster create' command line according to given
parameters, creates namespace if necessary, creates kubeconfig context (of
given k3d cluster and namespace) and switches to this config.
In examples below cluster name may be any. Special name 'AUTO' generates
cluster name from login name and checkout directory
- Simplest form - create cluster with HTTP and HTTPS ports exposed:
    $ helm/bin/k3d_cluster_create.py AUTO
  name also may be specified explicitly:
    $ helm/bin/k3d_cluster_create.py mycluster
- Expose http, https, ratdb ports:
    $ helm/bin/k3d_cluster_create.py --expose http,https,ratdb AUTO
- Explicitly specify HTTP port:
    $ helm/bin/k3d_cluster_create.py --expose http:8800 AUTO
- Use namespace different from default:
    $ helm/bin/k3d_cluster_create.py --namespace mynamespace AUTO
- Explicitly specify kubeconfig (e.g. when running from under 'kubie ctx'):
    $ helm/bin/k3d_cluster_create.py --kubeconfig ~/.kube/config AUTO
- Explicitly specify location static files (terrain databases, etc.):
    $ helm/bin/k3d_cluster_create.py --static /somewhere/rat_transfer AUTO
- Explicitly specify k3d registry (if more than one running):
    $ helm/bin/k3d_cluster_create.py --k3d_reg k3d-myreg.localhost:43649 AUTO
  or:
    $ helm/bin/k3d_cluster_create.py --k3d_reg :43649 AUTO
  or:
    $ helm/bin/k3d_cluster_create.py --k3d_reg myreg.localhost AUTO

Other k3d cluster operations do not need additional support, hence may be
performed directly:
- List k3d clusters:
    $ k3d cluster list
- Detailed information about k3d clusters:
    $ k3d cluster list -o yaml
- Delete cluster:
    $ helm/bin/k3d_cluster_create.py --delete AUTO
- Stop cluster:
    $ k3d cluster stop clustername
- (Re)start cluster:
    $ k3d cluster start clustername
"""


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    known_nodeports = get_known_nodeports(INT_HELM_REL_DIR)
    argument_parser = \
        argparse.ArgumentParser(
            description="Wrapper around 'k3d cluster create'",
            formatter_class=argparse.RawDescriptionHelpFormatter,
            epilog=EPILOG)
    argument_parser.add_argument(
        "--expose",
        metavar="NODEPORT1[:HOST_PORT1][,NODEPORT2[:HOST_PORT2]]...",
        action="append",
        help=f"Cluster nodeports to expose to host. NODEPORTn is either "
        f"nodeport value (integer), or key of values.yaml's "
        f"components.*.ports (descriptor should hav eunique name and nodePort "
        f"subkey). Known nodeport names: "
        f"{', '.join(sorted(known_nodeports.keys()))}). For this to work "
        f"ports should also be exposed during helm installation (--expose "
        f"switch of helm_install.py). HOST_PORTn are unused host ports (if "
        f"unspecified - some unused ports are chosen). This parameter may be "
        "specified more than once")
    argument_parser.add_argument(
        "--delete", action="store_true",
        help="Delete cluster if it is running. Not very logical, but makes "
        "things simple")
    argument_parser.add_argument(
        "--kubeconfig", metavar="KUBECONFIG_FILE",
        help="Kubeconfig file to use. By default current one is used, but if "
        "some context switching shell is in use (e.g. started by "
        "'kubie ctx'), current kubeconfig file is temporary, so the 'real' "
        "one should be specified explicitly")
    argument_parser.add_argument(
        "--namespace", metavar="NAMESPACE",
        help="Namespace to create. Default namespace is used by default")
    argument_parser.add_argument(
        "--static", metavar="STATIC_FILE_DIR",
        help="Static file directory (usually /.../rat_transfer). By default "
        "looks in well-known locations (see script's config file)")
    argument_parser.add_argument(
        "--k3d_reg", metavar="K3D_REGISTRY",
        help="K3d image registry to use. By default first (and only!) "
        "currently running k3d registry is used")
    argument_parser.add_argument(
        "CLUSTER",
        help="Cluster name (RFC1123: [a-z0-9-]+). Note that k3d will actually "
        "create cluster with 'k3d-' prefix, but in k3d commands cluster name "
        "should be specified without prefix. Use 'AUTO' to generate name from "
        "source checkout directory and username")

    if not argv:
        argument_parser.print_help()
        sys.exit(1)

    args = argument_parser.parse_args(argv)

    # Computing cluster name
    cluster = args.CLUSTER
    if cluster.startswith(K3D_PREFIX):
        cluster = cluster[len(K3D_PREFIX):]
    if cluster == AUTO_NAME:
        cluster = auto_name(kabob=True)

    # If cluster already running - let's delete it first
    cluster_existed = False
    for attempt in range(10):
        for cluster_info in \
                parse_json_output(["k3d", "cluster", "list", "-o", "json"]):
            if cluster_info["name"] == cluster:
                break
        else:
            break
        cluster_existed = True
        running_clause = "" if args.delete \
            else f"Cluster '{cluster}' currently running. "
        print(f"{running_clause}Attempt to delete #{attempt + 1}")
        execute(["k3d", "cluster", "delete", cluster])
    else:
        error(f"Cluster `{cluster}' still runs despite numerous attempts to "
              f"delete it")
    if args.delete:
        error_if(not cluster_existed, f"Cluster '{cluster}' not found")
        return

    # Inspecting kubeconfig
    env_kubeconfig = os.environ.get("KUBECONFIG")
    error_if((not args.kubeconfig) and env_kubeconfig and
             (not os.path.relpath(env_kubeconfig,
                                  tempfile.gettempdir()).startswith("..")),
             "Kubeconfig file should be specified explicitly, because current "
             "one seems to be temporary")

    # Looking for static directory
    def is_static_dir(dirname: str) -> bool:
        """ True if given directory is static file directory """
        return bool(dirname) and os.path.isdir(dirname) and \
            all(os.path.exists(os.path.join(dirname, f))
                for f in STATIC_FILE_EXPETED_CONTENT)

    static_dir = args.static
    if static_dir:
        error_if(not is_static_dir(static_dir),
                 f"Static file directory '{static_dir}' does not contain "
                 "expected files/directories")
    else:
        for kl in KNOWN_STATIC_FILE_LOCATIONS:
            if is_static_dir(kl):
                static_dir = kl
                break
        else:
            error("Static file directory (usually something like "
                  "/.../rat_transfer) not found in expected places. It should "
                  "be specified explicitly with --static parameter")

    # Looking for registry
    registry = cast(ImageRegistryInfo, parse_k3d_reg(args.k3d_reg)).pull

    # Computing port mappings
    conns = cast(str, execute(["netstat", "-latn"], return_output=True))
    used_ports: Set[int] = set()
    for line in conns.splitlines():
        m = re.match(r"^\s*\S+\s+\S+\s+\S+\s+\S+:(\d+)\s+", line)
        if m:
            used_ports.add(int(m.group(1)))

    def get_unused_port() -> int:
        for ret in range(30000, 65536):
            if ret not in used_ports:
                used_ports.add(ret)
                return ret
        error("All ports allocated?!?!")
        return 0  # Unreachable code

    MappingInfo = \
        NamedTuple(
            "MappingInfo",
            [("component", Optional[str]), ("port_name", Optional[str]),
             ("nodeport", int)])
    port_mappings: Dict[int, MappingInfo] = {}
    for mapping_strings in (args.expose or [",".join(DEFAULT_EXPOSE)]):
        for mapping_str in mapping_strings.split(","):
            component: Optional[str] = None
            port_name: Optional[str] = None
            m = re.match(r"^([^:]+)(:(\d+))?$", mapping_str)
            error_if(m is None,
                     f"Nodeport descriptor '{mapping_str}' has invalid format")
            assert m is not None
            if re.match(r"^\d+$", m.group(1)):
                nodeport = int(m.group(1))
            else:
                port_name = m.group(1)
                error_if(port_name not in known_nodeports,
                         f"Nodeport name '{port_name}' is not known. Please "
                         f"specify it numerically")
                nodeport = known_nodeports[port_name].port
                component = known_nodeports[port_name].component
            if m.group(3):
                hostport = int(m.group(3))
                error_if(hostport in port_mappings,
                         f"Port '{hostport}' mapped more than once")
                warning_if(hostport in used_ports,
                           f"Looks like host port '{hostport}' is in use")
                used_ports.add(hostport)
            else:
                hostport = get_unused_port()
            port_mappings[hostport] = \
                MappingInfo(component=component, port_name=port_name,
                            nodeport=nodeport)

    # If cluster already running - let's delete it first
    for attempt in range(10):
        for cluster_info in \
                parse_json_output(["k3d", "cluster", "list", "-o", "json"]):
            if cluster_info["name"] == cluster:
                break
        else:
            break
        print(f"Cluster '{cluster}' currently running. Attempt to delete "
              f"#{attempt + 1}")
        execute(["k3d", "cluster", "delete", cluster])
    else:
        error(f"Cluster `{cluster}' still runs despite numerous attempts to "
              f"delete it")

    # Creating cluster
    kubeconfig: Optional[str] = \
        expand_filename(args.kubeconfig) if args.kubeconfig else None
    execute(
        ["k3d", "cluster", "create", cluster, "--agents", "1",
         "-v", f"{static_dir}:/opt/afc/databases/rat_transfer",
         "-v", "/home:/hosthome", "--registry-use", registry,
         "--kubeconfig-switch-context", "--kubeconfig-update-default"] +
        (["--config", kubeconfig] if kubeconfig else []) +
        sum((["--port", f"{host_port}:{mapping_info.nodeport}@agent:0"]
             for host_port, mapping_info in port_mappings.items()), []))

    # Install stuff into cluster
    execute([INSTALL_PREREQUISITES_SCRIPT,
             "--context", f"{kubeconfig or ''}:k3d-{cluster}"] +
            INSTALL_PREREQUISITES_COMPONENTS)

    # Reporting results
    namespace_clause = f"namespace of '{args.namespace}'" if args.namespace \
        else "default namespace"
    print(f"Cluster '{cluster}' (aka '{K3D_PREFIX}{cluster}') created "
          f"in context '{K3D_PREFIX}{cluster}' that made current in "
          f"kubeconfig "
          f"`{args.kubeconfig or env_kubeconfig or '~/.kube/config'}' with "
          f"{namespace_clause}.\n"
          f"Host port mappings:")
    for host_port, mapping_info in port_mappings.items():
        info_str = \
            f" ({mapping_info.port_name} of {mapping_info.component})" \
            if mapping_info.port_name and mapping_info.component else ""
        print(f"  {host_port} -> {mapping_info.nodeport}{info_str}")


if __name__ == "__main__":
    main(sys.argv[1:])
