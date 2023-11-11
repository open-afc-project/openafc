#!/usr/bin/env python3
""" ULS data download control service healthcheck """

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# pylint: disable=unused-wildcard-import, wrong-import-order, wildcard-import
# pylint: disable=logging-fstring-interpolation, invalid-name, too-many-locals
# pylint: disable=too-many-branches, too-few-public-methods

import argparse
import datetime
import email.mime.text
import logging
import os
import pydantic
import smtplib
import sys
from typing import Any, cast, List, Optional, Set, Union

from uls_service_common import *


class Settings(pydantic.BaseSettings):
    """ Arguments from command lines - with their default values """

    status_dir: str = pydantic.Field(DEFAULT_STATUS_DIR, env="ULS_STATUS_DIR")
    smtp_info: Optional[str] = pydantic.Field(None, env="ULS_ALARM_SMTP_INFO")
    email_to: Optional[str] = pydantic.Field(None, env="ULS_ALARM_EMAIL_TO")
    beacon_email_to: Optional[str] = pydantic.Field(None, env="ULS_BEACON_EMAIL_TO")
    email_sender_location: Optional[str] = \
        pydantic.Field(None, env="ULS_ALARM_SENDER_LOCATION")
    alarm_email_interval_hr: Optional[float] = \
        pydantic.Field(None, env="ULS_ALARM_ALARM_INTERVAL_HR")
    beacon_email_interval_hr: Optional[str] = \
        pydantic.Field(None, env="ULS_ALARM_BEACON_INTERVAL_HR")
    download_attempt_max_age_health_hr: Optional[float] = \
        pydantic.Field(6, env="ULS_HEALTH_ATTEMPT_MAX_AGE_HR")
    download_success_max_age_health_hr: Optional[float] = \
        pydantic.Field(8, env="ULS_HEALTH_SUCCESS_MAX_AGE_HR")
    update_max_age_health_hr: Optional[float] = \
        pydantic.Field(40, env="ULS_HEALTH_UPDATE_MAX_AGE_HR")
    download_attempt_max_age_alarm_hr: Optional[float] = \
        pydantic.Field(None, env="ULS_ALARM_ATTEMPT_MAX_AGE_HR")
    download_success_max_age_alarm_hr: Optional[float] = \
        pydantic.Field(None, env="ULS_ALARM_SUCCESS_MAX_AGE_HR")
    region_update_max_age_alarm: Optional[str] = \
        pydantic.Field(None, env="ULS_ALARM_REG_UPD_MAX_AGE_HR")
    verbose: bool = pydantic.Field(False)
    force_success: bool = pydantic.Field(False)

    @pydantic.root_validator(pre=True)
    @classmethod
    def remove_empty(cls, v: Any) -> Any:
        """ Prevalidator that removes empty values (presumably from environment
        variables) to force use of defaults
        """
        if not isinstance(v, dict):
            return v
        for key, value in list(v.items()):
            if value is None:
                del v[key]
        return v


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
    to_list = to.split(",")
    msg["To"] = to

    try:
        smtp = smtplib.SMTP_SSL(**smtp_kwargs) if credentials.get("USE_SSL") \
            else smtplib.SMTP(**smtp_kwargs)
        if credentials.get("USE_TLS"):
            smtp.starttls()  # No idea how it would work without key/cert
        else:
            smtp.login(**login_kwargs)
        smtp.sendmail(msg["From"], to_list, msg.as_string())
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


def email_if_needed(status_storage: StatusStorage, settings: Any) -> None:
    """ Send alarm/beacon emails if needed

    Arguments:
    status_storage -- status storage
    settings       -- Settings object
    """
    if (settings.alarm_email_interval_hr is None) and \
            (settings.beacon_email_interval_hr is None):
        return
    service_birth = status_storage.read_milestone(StatusStorage.S.ServiceBirth)
    problems: List[str] = []
    email_to = settings.email_to
    if expired(
            event_td=status_storage.read_milestone(
                StatusStorage.S.DownloadStart) or service_birth,
            max_age_hr=settings.download_attempt_max_age_alarm_hr):
        problems.append(
            f"AFC ULS database download attempts were not taken for more than "
            f"{settings.download_attempt_max_age_alarm_hr} hours")
    if expired(
            event_td=status_storage.read_milestone(
                StatusStorage.S.DownloadSuccess) or service_birth,
            max_age_hr=settings.download_success_max_age_alarm_hr):
        problems.append(
            f"AFC ULS database download attempts were not succeeded for more "
            f"than {settings.download_attempt_max_age_alarm_hr} hours")
    reg_data_changes = \
        status_storage.read_reg_data_changes(StatusStorage.S.RegionUpdate)
    regions: Set[str] = set()
    for reg_hr in ((settings.region_update_max_age_alarm or "").split(",")
                   or []):
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

    loc = settings.email_sender_location
    if problems:
        message_subject = "AFC ULS Service encountering problems"
        nl = "\n"
        message_body = \
            (f"AFC ULS download service {('at ' + loc) if loc else ''} "
             f"encountered the following problems:\n{nl.join(problems)}\n\n")
        email_interval_hr = settings.alarm_email_interval_hr
        email_milestone = StatusStorage.S.AlarmSent
    else:
        message_subject = "AFC ULS Service works fine"
        message_body = \
            (f"AFC ULS download service {('at ' + loc) if loc else ''} "
             f"works fine\n\n")
        email_interval_hr = settings.beacon_email_interval_hr
        email_milestone = StatusStorage.S.BeaconSent
        if settings.beacon_email_to:
            email_to = settings.beacon_email_to

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
    send_email_smtp(smtp_info_filename=settings.smtp_info,
                    to=email_to, subject=message_subject,
                    body=message_body)
    status_storage.write_milestone(email_milestone)


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = argparse.ArgumentParser(
        description="ULS data download control service healthcheck")
    argument_parser.add_argument(
        "--status_dir", metavar="STATUS_DIR",
        help=f"Directory with control script status information"
        f"{env_help(Settings, 'status_dir')}")
    argument_parser.add_argument(
        "--smtp_info", metavar="SMTP_CREDENTIALS_FILE",
        help=f"SMTP credentials file. For its structure - see "
        f"NOTIFIER_MAIL.json secret in one of files in "
        f"tools/secrets/templates directory. If parameter not specified or "
        f"secret file is empty - no alarm/beacon emails will be sent"
        f"{env_help(Settings, 'smtp_info')}")
    argument_parser.add_argument(
        "--email_to", metavar="EMAIL",
        help=f"Email address to send alarms/beacons to. If parameter not "
        f"specified - no alarm/beacon emails will be sent"
        f"{env_help(Settings, 'email_to')}")
    argument_parser.add_argument(
        "--beacon_email_to", metavar="BEACON_EMAIL",
        help=f"Email address to send beacon information  notification to. If parameter not "
        f"specified - alarm email_to will used "
        f"{env_help(Settings, 'beacon_email_to')}")
    argument_parser.add_argument(
        "--email_sender_location", metavar="TEXT",
        help=f"Information on whereabouts of service that sent alarm/beacon "
        f"email{env_help(Settings, 'email_sender_location')}")
    argument_parser.add_argument(
        "--alarm_email_interval_hr", metavar="HOURS",
        help=f"Minimum interval (in hours) between alarm emails. Default is "
        f"to not send alarm emails"
        f"{env_help(Settings, 'alarm_email_interval_hr')}")
    argument_parser.add_argument(
        "--beacon_email_interval_hr", metavar="HOURS",
        help=f"Interval (in hours) between beacon (everything is OK) email "
        f"messages. Default is to not send beacon emails"
        f"{env_help(Settings, 'beacon_email_interval_hr')}")
    argument_parser.add_argument(
        "--download_attempt_max_age_health_hr", metavar="HOURS",
        help=f"Pronounce service unhealthy if no download attempts were made "
        f"for this number of hour"
        f"{env_help(Settings, 'download_attempt_max_age_health_hr')}")
    argument_parser.add_argument(
        "--download_success_max_age_health_hr", metavar="HOURS",
        help=f"Pronounce service unhealthy if no download attempts were "
        f"succeeded for this number of hours"
        f"{env_help(Settings, 'download_success_max_age_health_hr')}")
    argument_parser.add_argument(
        "--update_max_age_health_hr", metavar="HOURS",
        help=f"Pronounce service unhealthy if no ULS updates were made for "
        f"this number of hours"
        f"{env_help(Settings, 'update_max_age_health_hr')}")
    argument_parser.add_argument(
        "--download_attempt_max_age_alarm_hr", metavar="HOURS",
        help=f"Send email alarm if no download attempts were made for this "
        f"number of hours"
        f"{env_help(Settings, 'download_attempt_max_age_alarm_hr')}")
    argument_parser.add_argument(
        "--download_success_max_age_alarm_hr", metavar="HOURS",
        help=f"Send email alarm if no download attempts were succeeded for "
        f"this number of hours"
        f"{env_help(Settings, 'download_success_max_age_alarm_hr')}")
    argument_parser.add_argument(
        "--region_update_max_age_alarm",
        metavar="REG1:HOURS1[,REG2:HOURS2...]",
        help=f"Send alarm email if data for given regions (e.g. 'US', 'CA', "
        f"etc.) were not updated for given number of hours"
        f"{env_help(Settings, 'region_update_max_age_alarm')}")
    argument_parser.add_argument(
        "--verbose", action="store_true",
        help=f"Print detailed log information{env_help(Settings, 'verbose')}")
    argument_parser.add_argument(
        "--force_success", action="store_true",
        help="Don't return error exit code if container found to be unhealthy")

    settings: Settings = \
        cast(Settings, merge_args(settings_class=Settings,
                                  args=argument_parser.parse_args(argv)))

    setup_logging(verbose=settings.verbose)

    status_storage = StatusStorage(status_dir=settings.status_dir)

    status_storage.write_milestone(StatusStorage.S.HealthCheck)

    email_if_needed(status_storage=status_storage, settings=settings)

    last_start = status_storage.read_milestone(StatusStorage.S.ServiceStart)
    if last_start is None:
        logging.error("ULS downloader service not started")
        if not settings.force_success:
            sys.exit(1)

    if expired(event_td=status_storage.read_milestone(
               StatusStorage.S.DownloadStart),
               max_age_hr=settings.download_attempt_max_age_health_hr):
        logging.error(f"Last download attempt happened more than "
                      f"{settings.download_attempt_max_age_health_hr} hours "
                      f"ago or not at all")
        if not settings.force_success:
            sys.exit(1)

    if expired(event_td=status_storage.read_milestone(
               StatusStorage.S.DownloadSuccess),
               max_age_hr=settings.download_success_max_age_health_hr):
        logging.error(f"Last download success happened more than "
                      f"{settings.download_success_max_age_health_hr} hours "
                      f"ago or not at all")
        if not settings.force_success:
            sys.exit(1)

    if expired(event_td=status_storage.read_milestone(
               StatusStorage.S.SqliteUpdate),
               max_age_hr=settings.update_max_age_health_hr):
        logging.error(f"Last time ULS database was updated happened more than "
                      f"{settings.update_max_age_health_hr} hours ago "
                      f"or not at all")
        if not settings.force_success:
            sys.exit(1)


if __name__ == "__main__":
    main(sys.argv[1:])
