#!/usr/bin/env python3
# Operations around Kubernetes secret manifest files

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# pylint: disable=logging-fstring-interpolation, invalid-name, too-many-locals
# pylint: disable=too-many-branches, too-many-nested-blocks
# pylint: disable=too-many-statements

import argparse
import base64
import binascii
from collections.abc import Iterator
import json
import logging
import os
import sys
from typing import Any, Dict, List, NamedTuple, Optional, Set, Tuple, Union
import yaml

# Extensions for JSON-formatted secrets
JSON_EXTENSIONS = [".json"]
# Extensions for YAML-formatted secrets
YAML_EXTENSIONS = [".yaml", ".yml"]


def fatal(msg: str) -> None:
    """ Prints given msg as fatal message and exits with an error """
    logging.fatal(msg)
    sys.exit(1)


def fatal_if(cond: Any, msg: str) -> None:
    """ If condition evaluates to true prints given msg as fatal message and
    exits with an error """
    if cond:
        fatal(msg)


def get_yaml(filename: str, secret_key: str, secret_value: Optional[str]) \
        -> Tuple[bool, Optional[Union[List[Any], Dict[str, Any]]]]:
    """ Tries to decode secret as YAML-formatted

    Arguments:
    filename     -- Name of secret manifest/template file
    secret_key   -- Key inside the secret
    secret_value -- Value of secret

    Returns (success, optional_yaml_dictionary) tuple. 'success' is true if no
    decode was attempted (secret name does not have YAML extension) or if
    decode was successful """
    if (not secret_value) or \
            (os.path.splitext(secret_key)[1] not in YAML_EXTENSIONS):
        return (True, None)
    try:
        return (True, yaml.load(secret_value, Loader=yaml.CLoader))
    except yaml.YAMLError as ex:
        logging.error(f"File '{filename}' has incorrectly formatted YAML "
                      f"secret '{secret_key}': {ex}")
        return (False, None)


def get_json(filename: str, secret_key: str, secret_value: Optional[str]) \
        -> Tuple[bool, Optional[Union[List[Any], Dict[str, Any]]]]:
    """ Tries to decode secret as JSON-formatted

    Arguments:
    filename     -- Name of secret manifest/template file
    secret_key   -- Key inside the secret
    secret_value -- Value of secret

    Returns (success, optional_json_dictionary) tuple. 'success' is true if no
    decode was attempted (secret name does not have JSON extension) or if
    decode was successful """
    if (not secret_value) or \
            (os.path.splitext(secret_key)[1] not in JSON_EXTENSIONS):
        return (True, None)
    try:
        return (True, json.loads(secret_value))
    except json.JSONDecodeError as ex:
        logging.error(f"File '{filename}' has incorrectly formatted JSON "
                      f"secret '{secret_key}': {ex}")
        return (False, None)


def clean_secret(secret_value: Union[List[Any], Dict[str, Any], Any]) \
        -> Optional[Union[List[Any], Dict[str, Any], Any]]:
    """ Recursive function that replaces all scalar values in given
    scalar/list/dictionary object to Nones
    """
    if isinstance(secret_value, list):
        return [clean_secret(e) for e in secret_value]
    if isinstance(secret_value, dict):
        return {k: clean_secret(v) for k, v in secret_value.items()}
    return None


class ManifestInfo(NamedTuple):
    """ Information about secret manifest/template file """

    # File name
    filename: str

    # File formatting is valid
    valid: bool = True

    # Metadata name
    name: Optional[str] = None

    # Dictionary of file(base64) - formatted secrets
    data: Dict[str, Optional[bytes]] = {}

    # Dictionary of string-formatted secrets
    string_data: Dict[str, Optional[str]] = {}


def manifests(files: List[str], is_template: Optional[bool] = None) \
        -> "Iterator[ManifestInfo]":
    """ Reads and validates list of manifests

    Arguments:
    files       -- List of filenames
    is_template -- True for template, False for manifest, None if not known
    Returns sequence of ManifestInfo objects
    """
    assert files
    secret_names: Set[str] = set()
    secret_keys: Set[str] = set()
    for filename in files:
        if not os.path.isfile(filename):
            logging.error(f"Manifest file '{filename}' not found")
            yield ManifestInfo(filename=filename, valid=False)
        try:
            with open(filename, encoding="utf-8") as f:
                yaml_dicts = yaml.load_all(f.read(), Loader=yaml.CLoader)
        except OSError as ex:
            logging.error(f"Error reading manifest file '{filename}': {ex}")
            yield ManifestInfo(filename=filename, valid=False)
            continue
        except yaml.YAMLError as ex:
            logging.error(
                f"YAML parsing failed reading manifest file '{filename}': "
                f"{ex}")
            yield ManifestInfo(filename=filename, valid=False)
            continue
        for yaml_idx, yaml_dict in enumerate(yaml_dicts):
            valid = True
            name = yaml_dict.get("metadata", {}).get("name")
            if not name:
                logging.error(f"'metadata: name:' not found in secret "
                              f"#{yaml_idx + 1} of manifest file '{filename}'")
                valid = False
            if name in secret_names:
                logging.error(f"Secret name '{name}' encountered more than "
                              f"once")
                valid = False

            secret_names.add(name)
            if not yaml_dict.get("apiVersion"):
                logging.error(f"'apiVersion' field not found in manifest file "
                              f"'{filename}'")
                valid = False
            if yaml_dict.get("kind") != "Secret":
                logging.error(
                    f"'Manifest file '{filename}' is not of a 'kind: Secret'")
                valid = False
            elif not isinstance(name, str):
                logging.error(f"'metadata: name:' is no a string in manifest "
                              f"file '{filename}'")
                name = None
                valid = False
            data: Dict[str, Any] = yaml_dict.get("data", {})
            if not isinstance(data, dict):
                logging.error(f"'data' field of manifest file '{filename}' is "
                              f"not a dictionary")
                data = {}
                valid = False
            string_data: Dict[str, Any] = yaml_dict.get("stringData", {})
            if not isinstance(string_data, dict):
                logging.error(f"'stringData' field of manifest file "
                              f"'{filename}' is not a dictionary")
                string_data = {}
                valid = False
            valid = True
            decoded_data: Dict[str, Optional[bytes]] = {}
            for secret_key, secret_value in data.items():
                if secret_key in secret_keys:
                    logging.error(f"Secret key '{secret_key}' encountered "
                                  f"more than once")
                    valid = False
                secret_keys.add(secret_key)
                decoded_data[secret_key] = None
                if is_template is not None:
                    if is_template:
                        if secret_value is not None:
                            logging.error(f"Manifest template file "
                                          f"'{filename}' has nonempty value "
                                          f"for secret '{secret_key}'")
                            valid = False
                            continue
                    else:
                        if not isinstance(secret_value, str):
                            logging.error(f"Manifest file '{filename}' has "
                                          f"nonstring value for secret "
                                          f"'{secret_key}'")
                            valid = False
                        else:
                            try:
                                decoded_data[secret_key] = \
                                    base64.b64decode(secret_value)
                            except binascii.Error as ex:
                                logging.error(f"Manifest file '{filename}' "
                                              f"has incorrectly encoded "
                                              f"Base64 value for secret "
                                              f"'{secret_key}'")
                                valid = False
            for secret_key, secret_value in string_data.items():
                if secret_key in secret_keys:
                    logging.error(f"Secret key '{secret_key}' encountered "
                                  f"more than once")
                    valid = False
                secret_keys.add(secret_key)
                if secret_key in data:
                    logging.error(f"Manifest "
                                  f"{'template ' if is_template else ''} "
                                  f"file '{filename}' has duplicate secret "
                                  f"'{secret_key}'")
                    valid = False
                if is_template:
                    if secret_value is None:
                        continue
                    if not isinstance(secret_value, str):
                        logging.error(f"Manifest template file '{filename}' "
                                      f"has nonempty nonstring value for "
                                      f"secret '{secret_key}'")
                        valid = False
                        continue
                    for retrieve in (get_yaml, get_json):
                        v, content = retrieve(filename=filename,
                                              secret_key=secret_key,
                                              secret_value=secret_value)
                        valid &= v
                        if content is not None:
                            if content != clean_secret(content):
                                logging.error(
                                    f"Manifest template file '{filename}' has "
                                    f"nonempty field values in secret "
                                    f"'{secret_key}'")
                                valid = False
                            break
                    else:
                        logging.error(f"Manifest template file '{filename}' "
                                      f"has nonempty value for secret "
                                      f"'{secret_key}'")
                        valid = False
                else:
                    if not isinstance(secret_value, str):
                        logging.error(f"Manifest file '{filename}' has "
                                      f"nonstring value for secret "
                                      f"'{secret_key}'")
                        valid = False
                    for retrieve in (get_yaml, get_json):
                        v, _ = retrieve(filename=filename,
                                        secret_key=secret_key,
                                        secret_value=secret_value)
                        valid &= v
            yield ManifestInfo(
                filename=filename, valid=valid, name=name, data=decoded_data,
                string_data=string_data)


def check_manifests(files: List[str], is_template: Optional[bool],
                    is_local: bool) -> bool:
    """ Check list of manifest/template files for validity and mutual
    consistency

    Arguments:
    files       -- List of manifest/template files
    is_template -- True if files are templates, False if manifests, None if can
                   be any
    is_local    -- True if secret names may duplicate across files
    True if everything valid
    """
    manifest_names: Set[str] = set()
    secret_names: Set[str] = set()
    ret = True
    for mi in manifests(files=files, is_template=is_template):
        ret &= mi.valid
        if mi.name:
            if mi.name in manifest_names:
                logging.error(f"Duplicate manifest name '{mi.name}'")
                ret = False
            manifest_names.add(mi.name)
        if not is_local:
            for secret_name in (list(mi.data.keys() or []) +
                                list(mi.string_data.keys() or [])):
                if secret_name in secret_names:
                    logging.error(f"Secret '{secret_name}' found in more "
                                  f"than one file")
                    ret = False
    return ret


def do_check(args: Any) -> None:
    """Execute "check" command.

    Arguments:
    args -- Parsed command line arguments
    """
    if not check_manifests(files=args.MANIFEST, is_template=args.template,
                           is_local=args.local_secrets):
        sys.exit(1)


def do_list(args: Any) -> None:
    """Execute "list" command.

    Arguments:
    args -- Parsed command line arguments
    """
    manifest_list = list(manifests(files=args.MANIFEST))
    for mi in manifest_list:
        print(f"{mi.filename}{(' (' + mi.name + ')') if mi.name else ''}"
              f"{' - has errors' if not mi.valid else ''}")
        for secret_name in \
                sorted(set(mi.data.keys()) | set(mi.string_data.keys())):
            print(f"    {secret_name}")


def do_export(args: Any) -> None:
    """Execute "export" command.

    Arguments:
    args -- Parsed command line arguments
    """
    fatal_if(not check_manifests(files=args.MANIFEST,
                                 is_template=None if args.empty else False,
                                 is_local=False),
             "Errors in manifest(s), export not done")
    prefix = args.base_dir if args.base_dir is not None else args.secrets_dir
    fatal_if(args.compose_file and (prefix is None),
             "Neither --secrets_dir nor --base_dir specified")
    prefix = prefix or "."
    if args.secrets_dir:
        os.makedirs(args.secrets_dir, exist_ok=True)
    secrets: Dict[str, str] = {}
    for mi in manifests(files=args.MANIFEST):
        d: Dict[str, Any]
        for d, mode, encoding in [(mi.data, "wb", "ascii"),
                                  (mi.string_data, "w", "utf-8")]:
            for secret_name, secret_value in d.items():
                secrets[secret_name] = {"file": f"{prefix}/{secret_name}"}
                if args.secrets_dir is None:
                    continue
                try:
                    filename = os.path.join(args.secrets_dir, secret_name)
                    with open(filename, mode=mode, encoding=encoding) as f:
                        if not args.empty:
                            f.write(secret_value)
                except OSError as ex:
                    fatal(f"Error writing '{filename}': {ex}")
    if args.compose_file:
        try:
            with open(args.compose_file, mode="a", encoding="utf-8") as f:
                f.write(yaml.dump({"secrets": secrets}, indent=4))
        except OSError as ex:
            fatal(f"Error writing '{args.compose_file}': {ex}")


def do_compare_template(args: Any) -> None:
    """Execute "compare_template" command.

    Arguments:
    args -- Parsed command line arguments
    """
    fatal_if(not os.path.isdir(args.template_dir),
             f"Template directory '{args.template_dir}' not found")
    success = True
    manifest_filenames: Set[str] = set()
    for mi in manifests(files=args.MANIFEST, is_template=False):
        success &= mi.valid
        b = os.path.basename(mi.filename)
        if b in manifest_filenames:
            logging.error(f"More than one manifest file with name '{b}'")
            success = False
        manifest_filenames.add(b)
        template_filename = os.path.join(args.template_dir, b)
        if not os.path.isfile(template_filename):
            logging.error(f"Template file '{template_filename}' not found")
            success = False
            continue
        ti = list(manifests(files=[template_filename], is_template=True))[0]
        success &= ti.valid
        if mi.name and ti.name and (mi.name != ti.name):
            logging.error(f"Manifest and template files '{b}' have different "
                          f"'metadata: name:'")
            success = False
        this_secrets: Dict[str, Any]
        other_secrets: Dict[str, Any]
        for secret_type, this_name, other_name, this_secrets, other_secrets \
                in [("file secrets", "manifest", "template", mi.data, ti.data),
                    ("file secrets", "template", "manifest", ti.data, mi.data),
                    ("string secrets", "manifest", "template",
                     mi.string_data, ti.string_data),
                    ("string secrets", "template", "manifest",
                     ti.string_data, mi.string_data)]:
            d = set(this_secrets.keys()) - set(other_secrets.keys())
            if not d:
                continue
            logging.error(f"Manifest and template files '{b}' have different "
                          f"set of {secret_type}. Following secrets present "
                          f"in {this_name}, but absent in {other_name}: "
                          f"{', '.join(sorted(d))}.")
            success = False
        for s in sorted(mi.string_data.keys()):
            if s not in ti.string_data:
                continue
            for retrieve in (get_yaml, get_json):
                v, ms = retrieve(filename=mi.filename, secret_key=s,
                                 secret_value=mi.string_data[s])
                success &= v
                if not ms:
                    continue
                ms = clean_secret(ms)
                if s not in ti.string_data:
                    logging.error(f"Template file '{b}' does not contain "
                                  f"string secret '{s}'")
                    success = False
                    break
                v, ts = retrieve(filename=ti.filename, secret_key=s,
                                 secret_value=ti.string_data[s])
                success &= v
                if not ts:
                    logging.error(f"Template file '{b}' does not contain "
                                  f"inner structure for secret '{s}'")
                    success = False
                    break
                ts = clean_secret(ts)
                if ms != ts:
                    logging.error(
                        f"Manifest and template file '{b}' have different "
                        f"inner structures for secret '{s}'")
                    success = False
                break
    fatal_if(not success, "Comparison failed")


def do_help(args: Any) -> None:
    """Execute "help" command.

    Arguments:
    args -- Parsed command line arguments (also contains 'argument_parser' and
            'subparsers' fields)
    """
    if args.subcommand is None:
        args.argument_parser.print_help()
    else:
        args.subparsers.choices[args.subcommand].print_help()


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    switches_files = argparse.ArgumentParser(add_help=False)
    switches_files.add_argument(
        "MANIFEST", nargs="+",
        help="Kubernetes secret manifest file. Several may be specified")

    switches_template_dir = argparse.ArgumentParser(add_help=False)
    switches_template_dir.add_argument(
        "--template_dir", metavar="DIRECTORY", required=True,
        help="Directory for/with template secret files")

    argument_parser = argparse.ArgumentParser(
        description="Operations with AFC Kubernetes secret manifest files")
    subparsers = argument_parser.add_subparsers(dest="subcommand",
                                                metavar="SUBCOMMAND")

    parser_check = subparsers.add_parser(
        "check", parents=[switches_files],
        help="Check syntactical validity of secret manifest file() or "
        "template(s))")
    parser_check.add_argument(
        "--template", action="store_true",
        help="Manifest templates, should not have secret values "
        "(only secret names)")
    parser_check.add_argument(
        "--local_secrets", action="store_true",
        help="Allow different manifests have secrets with same name")
    parser_check.set_defaults(func=do_check)

    parser_list = subparsers.add_parser(
        "list", parents=[switches_files],
        help="List secret names")
    parser_list.set_defaults(func=do_list)

    parser_export = subparsers.add_parser(
        "export", parents=[switches_files],
        help="Write secrets to files, generate 'secrets' block for "
        "docker-compose.yaml")
    parser_export.add_argument(
        "--secrets_dir", metavar="DIRECTORY",
        help="Directory to write secrets to as individual files")
    parser_export.add_argument(
        "--empty", action="store_true",
        help="Make empty secret files. Template(s) may be used instead of "
        "manifest(s)")
    parser_export.add_argument(
        "--compose_file", metavar="FILENAME",
        help="File to write 'secrets' block. If file exists - block appended "
        "to it")
    parser_export.add_argument(
        "--base_dir", metavar="DIR_OR_MACRO",
        help="What to use as name of base directory in 'secrets' block. "
        "Default is to use --secrets_dir value")
    parser_export.set_defaults(func=do_export)

    parser_compare_template = subparsers.add_parser(
        "compare_template", parents=[switches_files],
        help="Verify that template(s) correspond to secret manifests")
    parser_compare_template.add_argument(
        "--template_dir", metavar="DIRECTORY", required=True,
        help="Directory containing template files. Template file(s) should "
        "have same name as manifest file(s). This parameter is mandatory")
    parser_compare_template.set_defaults(func=do_compare_template)

    parser_help = subparsers.add_parser(
        "help", add_help=False,
        help="Prints help on given subcommand")
    parser_help.add_argument(
        "subcommand", metavar="SUBCOMMAND", nargs="?",
        choices=subparsers.choices,
        help="Name of subcommand to print help about (use " +
        "\"%(prog)s --help\" to get list of all subcommands)")
    parser_help.set_defaults(func=do_help, subparsers=subparsers,
                             argument_parser=argument_parser)

    if not argv:
        argument_parser.print_help()
        sys.exit(1)

    # Set up logging
    console_handler = logging.StreamHandler()
    console_handler.setFormatter(
        logging.Formatter(
            f"{os.path.basename(__file__)}. %(levelname)s: %(message)s"))
    logging.getLogger().addHandler(console_handler)
    logging.getLogger().setLevel(logging.INFO)

    args = argument_parser.parse_args(argv)
    args.func(args)


if __name__ == "__main__":
    main(sys.argv[1:])
