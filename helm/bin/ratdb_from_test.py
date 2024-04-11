#!/usr/bin/env python3
""" Fills Kubernetes ratdb from regression tests (like run_srvr.sh) """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=too-many-statements, too-many-locals, invalid-name

import argparse
import json
import os
import random
import string
import sys
import tempfile
from typing import cast, List, Optional

from k3d_lib import error, execute, parse_kubecontext, ROOT_DIR

POD_NAME_STEMS = ["rat-server", "webui"]
TESTS_DOCKERFILE = "tests/Dockerfile"


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = \
        argparse.ArgumentParser(
            description="Fills Kubernetes ratdb from regression tests")
    argument_parser.add_argument(
        "--namespace", metavar="NAMESPACE",
        help="Namespace rat_server pod runs in. If neither this nor --context "
        "parameter specified the current/default is used")
    argument_parser.add_argument(
        "--context", metavar="[CONFIG_FILE][:CONTEXT]",
        help="Kubeconfig and/or context to use. Curent is used if not "
        "specified")
    argument_parser.add_argument(
        "--pod_stem", metavar="POD_NAME_STEM",
        help=f"Substring rat_server pod name must contain. Default is to try "
        f"these: {', '.join(POD_NAME_STEMS)}")

    args = argument_parser.parse_args(argv)

    # Filling in kubectl namespace-related arguments
    namespace_args = ""
    kubeconfig, kubecontext = parse_kubecontext(args.context)
    for switch, value in [("--namespace", args.namespace),
                          ("--kubeconfig", kubeconfig),
                          ("--context", kubecontext)]:
        if not value:
            continue
        if not namespace_args:
            namespace_args = " "
        namespace_args += f"{switch} {value}"

    # Finding pod that runs rat_server
    pod_stems = [args.pod_stem] if args.pod_stem else POD_NAME_STEMS
    pods_json = execute(f"""\
kubectl get pods -o json{namespace_args}""",
                        return_output=True)
    pods_dict = json.loads(cast(str, pods_json))
    for pod in pods_dict["items"]:
        pod_name = pod["metadata"]["name"]
        if any(stem in pod_name for stem in pod_stems):
            break
    else:
        error("rat_server pod not found among running ones")

    random_tag = f"temp_{''.join(random.choices(string.ascii_lowercase, k=8))}"
    test_image_name = f"temp_rtest:{random_tag}"
    test_container_name = f"temp_rtest_{random_tag}"
    cfg_file: Optional[str] = None
    try:
        fd, cfg_file = tempfile.mkstemp()
        os.close(fd)
        assert cfg_file is not None
        execute(f"""\
docker build . -t {test_image_name} -f {TESTS_DOCKERFILE}""",
                cwd=ROOT_DIR)
        execute(f"""\
docker run --name {test_container_name} {test_image_name} --cmd exp_adm_cfg \
    --outfile /export_admin_cfg.json
docker cp {test_container_name}:/export_admin_cfg.json {cfg_file}
docker container rm {test_container_name}
docker image rm {test_image_name}
kubectl cp{namespace_args} {cfg_file} {pod_name}:/tmp/export_admin_cfg.json
kubectl exec{namespace_args} {pod_name} -- rat-manage-api db-create
kubectl exec{namespace_args} {pod_name} -- rat-manage-api cfg add \
    src=/tmp/export_admin_cfg.json
kubectl exec{namespace_args} {pod_name} -- rat-manage-api user create \
    --role Super --role Admin --role AP --role Analysis --org fcc \
    "admin@afc.com" openafc
kubectl exec{namespace_args} {pod_name} -- rat-manage-api cert_id create \
    --location 3 --cert_id FsDownloaderCertIdUS \
    --ruleset_id US_47_CFR_PART_15_SUBPART_E
kubectl exec{namespace_args} {pod_name} -- rat-manage-api cert_id create \
    --location 3 --cert_id FsDownloaderCertIdCA --ruleset_id CA_RES_DBS-06
kubectl exec{namespace_args} {pod_name} -- rat-manage-api cert_id create \
    --location 3 --cert_id FsDownloaderCertIdBR \
    --ruleset_id BRAZIL_RULESETID""")
    finally:
        if cfg_file:
            os.unlink(cfg_file)


if __name__ == "__main__":
    main(sys.argv[1:])
