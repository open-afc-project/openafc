#!/usr/bin/env python3
# ULS data download control service healthcheck

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# pylint: disable=unused-wildcard-import, wrong-import-order, wildcard-import
# pylint: disable=logging-fstring-interpolation, invalid-name, too-many-locals
# pylint: disable=too-many-branches

import argparse
import datetime
import email.mime.text
import logging
import os
import smtplib
import sys
from typing import Any, List, Optional, Set, Union

from uls_service_common import *

# Default value for download_attempt_max_age_health_hr
DEFAULT_ATTEMPT_AGE_HR = 6.

# Default value for download_success_max_age_health_hr
DEFAULT_SUCCESS_AGE_HR = 8.

# Default value for --update_max_age_health_hr
DEFAULT_UPDATE_AGE_HR = 40.


def send_email_smtp(smtp_info_filename: Optional[str], to: Optional[str],
                    subject: str, body: str) -> None:
    """ Send an email via SMTP

    Arguments:
    smtp_info -- Optional name of JSON file containing SMTP credentials
    to        -- Optional recipient email
    subject   -- Message subject
    body      -- Message body
    """
    if not (smtp_info_filename and to):
        return
    error_if(not os.path.isfile(smtp_info_filename),
             f"SMTP credentials file '{smtp_info_filename}' not found")
    with open(smtp_info_filename, mode="rb") as f:
        smtp_info_content = f.read()
    if not smtp_info_content:
        return
    try:
        credentials = json.loads(smtp_info_content)
    except json.JSONDecodeError as ex:
        error(f"Invalid format of '{smtp_info_filename}': {ex}")
    smtp_kwargs: Dict[str, Any] = {}
    login_kwargs: Dict[str, Any] = {}
    for key, key_type, kwargs, arg_name in \
            [("SERVER", str, smtp_kwargs, "host"),
             ("PORT", int, smtp_kwargs, "port"),
             ("USERNAME", str, login_kwargs, "user"),
             ("PASSWORD", str, login_kwargs, "password")]:
        value = credentials.get(key)
        if value is None:
            continue
        error_if(not isinstance(value, key_type),
                 f"Invalid type for '{key}' in '{smtp_info_filename}'")
        kwargs[arg_name] = value
    error_if("user" not in login_kwargs,
             f"`USERNAME' (sender email) not found in '{smtp_info_filename}'")
    msg = email.mime.text.MIMEText(body)
    msg["Subject"] = subject
    msg["From"] = login_kwargs["user"]
    msg["To"] = to
    try:
        smtp = smtplib.SMTP_SSL(**smtp_kwargs) if credentials.get("USE_SSL") \
            else smtplib.SMTP(**smtp_kwargs)
        if credentials.get("USE_TLS"):
            smtp.starttls()  # No idea how it would work without key/cert
        else:
            smtp.login(**login_kwargs)
        smtp.sendmail(msg["From"], [msg["To"]], msg.as_string())
        smtp.quit()
    except smtplib.SMTPException as ex:
        error(f"Email send failure: {ex}")


def expired(event_td: Optional[datetime.datetime],
            max_age_hr: Optional[Union[int, float]]) -> bool:
    """ True if given amount of time expired since given event

    Arguments:
    event_td   -- Optional event timedate. None means that yes, expired (unless
                  timeout is None)
    max_age_hr -- Optional timeout in hours. None means no, not expired
    Returns true if timeout expired
    """
    if max_age_hr is None:
        return False
    if event_td is None:
        return True
    return (datetime.datetime.now() - event_td) > \
        datetime.timedelta(hours=max_age_hr)


def email_if_needed(status_storage: StatusStorage, args: Any) -> None:
    """ Send alarm/beacon emails if needed

    Arguments:
    status_storage -- status storage
    args           -- Parsed command line arguments (with timeouts and email
                      parameters)
    """
    if (args.alarm_email_interval_hr is None) and \
            (args.beacon_email_interval_hr is None):
        return
    service_birth = status_storage.read_milestone(StatusStorage.S.ServiceBirth)
    problems: List[str] = []
    if expired(
            event_td=status_storage.read_milestone(
                StatusStorage.S.DownloadStart) or service_birth,
            max_age_hr=args.download_attempt_max_age_alarm_hr):
        problems.append(
            f"AFC ULS database download attempts were not taken for more than "
            f"{args.download_attempt_max_age_alarm_hr} hours")
    if expired(
            event_td=status_storage.read_milestone(
                StatusStorage.S.DownloadSuccess) or service_birth,
            max_age_hr=args.download_success_max_age_alarm_hr):
        problems.append(
            f"AFC ULS database download attempts were not succeeded for more "
            f"than {args.download_attempt_max_age_alarm_hr} hours")
    reg_data_changes = \
        status_storage.read_reg_data_changes(StatusStorage.S.RegionUpdate)
    regions: Set[str] = set()
    for reg_hr in (args.region_update_max_age_alarm or []):
        if not reg_hr:
            continue
        error_if(":" not in reg_hr,
                 f"Invalid format of '--region_update_max_age_alarm "
                 f"{reg_hr}': no colon found")
        reg, hr = reg_hr.split(":", maxsplit=1)
        regions.add(reg)
        try:
            max_age_hr = float(hr)
        except ValueError:
            error(f"Invalid value for hours in '--region_update_max_age_alarm "
                  f"{reg_hr}'")
        if expired(event_td=reg_data_changes.get(reg) or service_birth,
                   max_age_hr=max_age_hr):
            problems.append(
                f"No new data for region '{reg}' were downloaded for more "
                f"than {max_age_hr} hours")

    external_files_problems = \
        ExtParamFilesChecker(status_storage=status_storage).get_problems()
    if external_files_problems:
        problems.append("External parameters files synchronization problems:")
        problems += [("  " + efp) for efp in external_files_problems]

    loc = args.email_sender_location
    if problems:
        message_subject = "AFC ULS Service encountering problems"
        nl = "\n"
        message_body = \
            (f"AFC ULS download service {('at ' + loc) if loc else ''} "
             f"encountered the following problems:\n{nl.join(problems)}\n\n")
        email_interval_hr = args.alarm_email_interval_hr
        email_milestone = StatusStorage.S.AlarmSent
    else:
        message_subject = "AFC ULS Service works fine"
        message_body = \
            (f"AFC ULS download service {('at ' + loc) if loc else ''} "
             f"works fine\n\n")
        email_interval_hr = args.beacon_email_interval_hr
        email_milestone = StatusStorage.S.BeaconSent

    if not expired(
            event_td=status_storage.read_milestone(email_milestone),
            max_age_hr=email_interval_hr):
        return

    message_body += "Overall service state is as follows:\n"
    for state, heading in \
            [(StatusStorage.S.ServiceBirth,
              "First time service was started in: "),
             (StatusStorage.S.ServiceStart,
              "Last time service was started in: "),
             (StatusStorage.S.DownloadStart,
              "Last ULS download attempt was taken in: "),
             (StatusStorage.S.DownloadSuccess,
              "Last ULS download attempt succeeded in: "),
             (StatusStorage.S.SqliteUpdate,
              "Last time ULS database was updated in: ")]:
        et = status_storage.read_milestone(state)
        message_body += \
            f"{heading}{'Unknown' if et is None else et.isoformat()}\n"
    for reg in sorted(set(reg_data_changes.keys() | regions)):
        et = reg_data_changes.get(reg)
        message_body += \
            (f"ULS data for region '{reg}' last time updated in: "
             f"{'Unknown' if et is None else et.isoformat()}")
    send_email_smtp(smtp_info_filename=args.smtp_info, to=args.email_to,
                    subject=message_subject, body=message_body)
    status_storage.write_milestone(email_milestone)


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = argparse.ArgumentParser(
        description="ULS data download control service healthcheck")
    argument_parser.add_argument(
        "--status_dir", metavar="STATUS_DIR", default=DEFAULT_STATUS_DIR,
        type=docker_arg_type(str, default=DEFAULT_STATUS_DIR),
        help=f"Directory with control script status information. Default is "
        f"'{DEFAULT_STATUS_DIR}'")
    argument_parser.add_argument(
        "--smtp_info", metavar="SMTP_CREDENTIALS_FILE",
        help="SMTP credentials file. For its structure - see "
        "NOTIFIER_MAIL.json secret in one of files in tools/secrets/templates "
        "directory. If parameter not specified or secret file is empty - no "
        "alarm/beacon emails will be sent")
    argument_parser.add_argument(
        "--email_to", metavar="EMAIL",
        help="Email address to send alarms/beacons to. If parameter not "
        "specified - no alarm/beacon emails will be sent")
    argument_parser.add_argument(
        "--email_sender_location", metavar="TEXT",
        type=docker_arg_type(str),
        help="Information on whereabouts of service that sent alarm/beacon "
        "email")
    argument_parser.add_argument(
        "--alarm_email_interval_hr", metavar="HOURS",
        type=docker_arg_type(float),
        help="Minimum interval (in hours) between alarm emails. Default is to "
        "not send alarm emails")
    argument_parser.add_argument(
        "--beacon_email_interval_hr", metavar="HOURS",
        type=docker_arg_type(float),
        help="Interval (in hours) between beacon (everything is OK) email "
        "messages. Default is to not send beacon emails")
    argument_parser.add_argument(
        "--download_attempt_max_age_health_hr", metavar="HOURS",
        default=DEFAULT_ATTEMPT_AGE_HR,
        type=docker_arg_type(float, default=DEFAULT_ATTEMPT_AGE_HR),
        help=f"Pronounce service unhealthy if no download attempts were made "
        f"for this number of hours. Default is {DEFAULT_ATTEMPT_AGE_HR}")
    argument_parser.add_argument(
        "--download_success_max_age_health_hr", metavar="HOURS",
        default=DEFAULT_SUCCESS_AGE_HR,
        type=docker_arg_type(float, default=DEFAULT_SUCCESS_AGE_HR),
        help=f"Pronounce service unhealthy if no download attempts were "
        "succeeded for this number of hours. Default is "
        f"{DEFAULT_SUCCESS_AGE_HR}")
    argument_parser.add_argument(
        "--update_max_age_health_hr", metavar="HOURS",
        default=DEFAULT_UPDATE_AGE_HR,
        type=docker_arg_type(float, default=DEFAULT_UPDATE_AGE_HR),
        help=f"Pronounce service unhealthy if no ULS updates were made for "
        f"this number of hours. Default is {DEFAULT_UPDATE_AGE_HR}")
    argument_parser.add_argument(
        "--download_attempt_max_age_alarm_hr", metavar="HOURS",
        type=docker_arg_type(float),
        help="Send email alarm if no download attempts were made for this "
        "number of hours")
    argument_parser.add_argument(
        "--download_success_max_age_alarm_hr", metavar="HOURS",
        type=docker_arg_type(float),
        help="Send email alarm if no download attempts were succeeded for "
        "this number of hours")
    argument_parser.add_argument(
        "--region_update_max_age_alarm", metavar="REG:HOURS", action="append",
        default=[], type=docker_arg_type(str),
        help="Send alarm email if data for given region (e.g. 'US', 'CA', "
        "etc.) was not updated for given number of hours. This parameter may "
        "be specified more than once")
    argument_parser.add_argument(
        "--verbose", nargs="?", metavar="[YES/NO]", const=True,
        type=docker_arg_type(bool, default=False),
        help="Print detailed log information")

    args = argument_parser.parse_args(argv)

    setup_logging(verbose=args.verbose)

    status_storage = StatusStorage(status_dir=args.status_dir)

    email_if_needed(status_storage=status_storage, args=args)

    last_start = status_storage.read_milestone(StatusStorage.S.ServiceStart)
    if last_start is None:
        logging.error("ULS downloader service not started")
        sys.exit(1)

    if expired(event_td=status_storage.read_milestone(
               StatusStorage.S.DownloadStart),
               max_age_hr=args.download_attempt_max_age_health_hr):
        logging.error(f"Last download attempt happened more than "
                      f"{args.download_attempt_max_age_health_hr} hours ago "
                      f"or not at all")
        sys.exit(1)

    if expired(event_td=status_storage.read_milestone(
               StatusStorage.S.DownloadSuccess),
               max_age_hr=args.download_success_max_age_health_hr):
        logging.error(f"Last download success happened more than "
                      f"{args.download_success_max_age_health_hr} hours ago "
                      f"or not at all")
        sys.exit(1)

    if expired(event_td=status_storage.read_milestone(
               StatusStorage.S.SqliteUpdate),
               max_age_hr=args.update_max_age_health_hr):
        logging.error(f"Last time ULS database was updated happened more than "
                      f"{args.update_max_age_health_hr} hours ago "
                      f"or not at all")
        sys.exit(1)


if __name__ == "__main__":
    main(sys.argv[1:])
