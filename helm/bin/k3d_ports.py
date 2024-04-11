#!/usr/bin/env python3
""" List port mappings for AFC k3d cluster """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=too-many-locals, invalid-name, too-many-branches

import argparse
import re
import sys
from typing import Dict, List

from k3d_lib import auto_name, AUTO_NAME, error, error_if, \
    get_known_nodeports, INT_HELM_REL_DIR, K3D_PREFIX, parse_json_output


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = \
        argparse.ArgumentParser(
            description="List port mappings of (AFC) k3d cluster")
    argument_parser.add_argument(
        "CLUSTER",
        help="K3d cluster name. 'AUTO' for cluster name made of login name "
        "and checkout directory. 'CURRENT' for current cluster")

    if not argv:
        argument_parser.print_help()
        sys.exit(1)

    args = argument_parser.parse_args(argv)

    # Computing cluster name
    if args.CLUSTER == "CURRENT":
        current_cfg = \
            parse_json_output(
                ["kubectl", "config", "view", "--minify", "-o", "json"])
        clusters = current_cfg.get("clusters", [])
        error_if(len(clusters) != 1,
                 "Can't determine a current cluster from kubeconfig")
        kubeconfig_cluster_name = clusters[0].get("name", "")
        error_if(not kubeconfig_cluster_name.startswith(K3D_PREFIX),
                 f"Current cluster '{kubeconfig_cluster_name}' is not a k3d "
                 f"cluster")
        cluster = kubeconfig_cluster_name[len(K3D_PREFIX):]
    elif args.CLUSTER == AUTO_NAME:
        cluster = auto_name(kabob=True)
    else:
        cluster = args.CLUSTER

    # Obtaining mappings from cluster
    host_to_node: Dict[int, int] = {}
    for cluster_info in \
            parse_json_output(["k3d", "cluster", "list", "-o", "json"],
                              echo=False):
        if cluster_info["name"] != cluster:
            continue
        for node_info in cluster_info["nodes"]:
            for nodeport_key, mappings in \
                    node_info.get("portMappings", {}).items():
                m = re.match(r"^(\d+)/tcp$", nodeport_key)
                if not m:
                    continue
                nodeport = int(m.group(1))
                for mapping_info in mappings:
                    host_port_str = mapping_info.get("HostPort")
                    if host_port_str:
                        host_to_node[int(host_port_str)] = nodeport
        break
    else:
        error(f"Cluster '{cluster}' not found")

    known_nodeports = get_known_nodeports(INT_HELM_REL_DIR)

    # Printing the result
    for host_port in sorted(host_to_node.keys()):
        nodeport = host_to_node[host_port]
        for nodeport_name, nodeport_info in known_nodeports.items():
            if nodeport_info.port == nodeport:
                print(f"{host_port} -> {nodeport}: {nodeport_name} of "
                      f"{nodeport_info.component}")
                break
        else:
            print(f"{host_port} -> {nodeport}")


if __name__ == "__main__":
    main(sys.argv[1:])
