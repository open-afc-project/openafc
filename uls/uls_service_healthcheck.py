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
# pylint: disable=too-many-statements

import argparse
import datetime
import email.mime.text
import logging
import os
import pydantic
import smtplib
import sqlalchemy as sa
import sys
from typing import Any, cast, List, NamedTuple, Optional, Set, Union

from pydantic_utils import env_help, merge_args
from secret_utils import safe_dsn
from uls_service_common import *
from uls_service_state_db import AlarmType, CheckType, DownloaderMilestone, \
    LogType, StateDb


class SmtpSettings(NamedTuple):
    """ SMTP server settings """
    server: Optional[str]
    port: Optional[int]
    username: Optional[str]
    password_file: Optional[str]
    ssl: bool
    tls: bool

    @property
    def password(self) -> Optional[str]:
        """ SMTP password (None if none) """
        if not (self.password_file and os.path.isfile(self.password_file)):
            return None
        with open(self.password_file, encoding="ascii") as f:
            return f.read() or None

    def is_specified(self) -> bool:
        """ True if parameters are seemingly specified """
        return bool(self.server) and bool(self.username)


class Settings(pydantic.BaseSettings):
    """ Arguments from command lines - with their default values """

    service_state_db_dsn: str = \
        pydantic.Field(..., env="ULS_SERVICE_STATE_DB_DSN")
    service_state_db_password_file: Optional[str] = \
        pydantic.Field(None, env="ULS_SERVICE_STATE_DB_PASSWORD_FILE")
    smtp_server: Optional[str] = pydantic.Field(None, env="ULS_SMTP_SERVER")
    smtp_port: Optional[int] = pydantic.Field(None, env="ULS_SMTP_PORT")
    smtp_username: Optional[str] = \
        pydantic.Field(None, env="ULS_SMTP_USERNAME")
    smtp_password_file: Optional[str] = \
        pydantic.Field(None, env="ULS_SMTP_PASSWORD_FILE")
    smtp_ssl: bool = pydantic.Field(False, env="ULS_SMTP_SSL")
    smtp_tls: bool = pydantic.Field(False, env="ULS_SMTP_TLS")
    email_to: Optional[str] = pydantic.Field(None, env="ULS_ALARM_EMAIL_TO")
    beacon_email_to: Optional[str] = \
        pydantic.Field(None, env="ULS_BEACON_EMAIL_TO")
    email_sender_location: Optional[str] = \
        pydantic.Field(None, env="ULS_ALARM_SENDER_LOCATION")
    alarm_email_interval_hr: Optional[float] = \
        pydantic.Field(None, env="ULS_ALARM_ALARM_INTERVAL_HR")
    beacon_email_interval_hr: Optional[float] = \
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
    print_email: bool = pydantic.Field(False)

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


def send_email_smtp(smtp_settings: SmtpSettings, to: Optional[str],
                    subject: str, body: str) -> None:
    """ Send an email via SMTP

    Arguments:
    smtp_settings -- SmtpSettings object
    to            -- Optional recipient email
    subject       -- Message subject
    body          -- Message body
    """
    if not (smtp_settings.is_specified() and to):
        return
    smtp_kwargs: Dict[str, Any] = {}
    login_kwargs: Dict[str, Any] = {}
    for attr, kwargs, arg_name in [("server", smtp_kwargs, "host"),
                                   ("port", smtp_kwargs, "port"),
                                   ("username", login_kwargs, "user"),
                                   ("password", login_kwargs, "password")]:
        value = getattr(smtp_settings, attr)
        if value is not None:
            kwargs[arg_name] = value
    msg = email.mime.text.MIMEText(body)
    msg["Subject"] = subject
    msg["From"] = login_kwargs["user"]
    to_list = to.split(",")
    msg["To"] = to

    try:
        with (smtplib.SMTP_SSL(**smtp_kwargs) if smtp_settings.ssl
              else smtplib.SMTP(**smtp_kwargs)) as smtp:
            if smtp_settings.tls:
                smtp.starttls()
            smtp.login(**login_kwargs)
            smtp.sendmail(msg["From"], to_list, msg.as_string())
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


def email_if_needed(state_db: StateDb, settings: Any) -> None:
    """ Send alarm/beacon emails if needed

    Arguments:
    state_db    -- StateDb object
    settings    -- Settings object
    """
    if (settings.alarm_email_interval_hr is None) and \
            (settings.beacon_email_interval_hr is None):
        return
    service_birth = \
        state_db.read_milestone(DownloaderMilestone.ServiceBirth).get(None)

    ProblemInfo = \
        NamedTuple("ProblemInfo",
                   [("alarm_type", AlarmType), ("alarm_reason", str),
                    ("message", str)])

    problems: List[ProblemInfo] = []
    email_to = settings.email_to
    if expired(
            event_td=state_db.read_milestone(
                DownloaderMilestone.DownloadStart).get(None, service_birth),
            max_age_hr=settings.download_attempt_max_age_alarm_hr):
        problems.append(
            ProblemInfo(
                alarm_type=AlarmType.MissingMilestone,
                alarm_reason=DownloaderMilestone.DownloadStart.name,
                message=f"AFC ULS database download attempts were not taken "
                f"for more than {settings.download_attempt_max_age_alarm_hr} "
                f"hours"))
    if expired(
            event_td=state_db.read_milestone(
                DownloaderMilestone.DownloadSuccess).get(None, service_birth),
            max_age_hr=settings.download_success_max_age_alarm_hr):
        problems.append(
            ProblemInfo(
                alarm_type=AlarmType.MissingMilestone,
                alarm_reason=DownloaderMilestone.DownloadSuccess.name,
                message=f"AFC ULS database download attempts were not "
                f"succeeded for more than "
                f"{settings.download_success_max_age_alarm_hr} hours"))
    reg_data_changes = \
        state_db.read_milestone(DownloaderMilestone.RegionChanged)
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
        if expired(event_td=reg_data_changes.get(reg, service_birth),
                   max_age_hr=max_age_hr):
            problems.append(
                ProblemInfo(
                    alarm_type=AlarmType.MissingMilestone,
                    alarm_reason=DownloaderMilestone.RegionChanged.name,
                    message=f"No new data for region '{reg}' were downloaded "
                    f"for more than {max_age_hr} hours"))
    check_prefixes = \
        {CheckType.ExtParams: "External parameters synchronization problem",
         CheckType.FsDatabase: "FS database validity check problem"}
    for check_type, check_infos in state_db.read_check_results().items():
        prefix = check_prefixes[check_type]
        for check_info in check_infos:
            if check_info.errmsg is None:
                continue
            problems.append(
                ProblemInfo(
                    alarm_type=AlarmType.FailedCheck,
                    alarm_reason=check_type.name,
                    message=f"{prefix}: {check_info.errmsg}"))

    loc = settings.email_sender_location

    alarms_cleared = False
    if problems:
        message_subject = "AFC ULS Service encountering problems"
        nl = "\n"
        message_body = \
            (f"AFC ULS download service {('at ' + loc) if loc else ''} "
             f"encountered the following problems:\n"
             f"{nl.join(problem.message for problem in problems)}\n\n")
        email_interval_hr = settings.alarm_email_interval_hr
        email_milestone = DownloaderMilestone.AlarmSent
    else:
        if state_db.read_alarm_reasons():
            alarms_cleared = True
            message_subject = "AFC FS Service problems resolved"
        else:
            message_subject = "AFC FS Service works fine"
        message_body = \
            (f"AFC FS download service {('at ' + loc) if loc else ''} "
             f"works fine\n\n")
        email_interval_hr = settings.beacon_email_interval_hr
        email_milestone = DownloaderMilestone.BeaconSent
        if settings.beacon_email_to:
            email_to = settings.beacon_email_to

    if (not alarms_cleared) and \
            (not expired(
                event_td=state_db.read_milestone(email_milestone).get(None),
                max_age_hr=email_interval_hr)):
        return

    message_body += "Overall service state is as follows:\n"
    for state, heading in \
            [(DownloaderMilestone.ServiceBirth,
              "First time service was started in: "),
             (DownloaderMilestone.ServiceStart,
              "Last time service was started in: "),
             (DownloaderMilestone.DownloadStart,
              "Last FS download attempt was taken in: "),
             (DownloaderMilestone.DownloadSuccess,
              "Last FS download attempt succeeded in: "),
             (DownloaderMilestone.DbUpdated,
              "Last time ULS database was updated in: ")]:
        et = state_db.read_milestone(state).get(None)
        message_body += \
            f"{heading}{'Unknown' if et is None else et.isoformat()}"
        if et is not None:
            message_body += f" ({datetime.datetime.now() - et} ago)"
        message_body += "\n"

    for reg in sorted(set(cast(str, r) for r in reg_data_changes.keys()) |
                      regions):
        et = reg_data_changes.get(reg)
        message_body += \
            (f"FS data for region '{reg}' last time updated in: "
             f"{'Unknown' if et is None else et.isoformat()}")
        if et is not None:
            message_body += f" ({datetime.datetime.now() - et} ago)"
        message_body += "\n"
    if email_milestone == DownloaderMilestone.AlarmSent:
        log_info = state_db.read_last_log(log_type=LogType.Last)
        if log_info:
            message_body += \
                f"\nDownload log of {log_info.timestamp.isoformat()}:\n" + \
                f"{log_info.text}\n"

    if settings.print_email:
        print(f"SUBJECT: {message_subject}\nBody:\n{message_body}")
    else:
        send_email_smtp(
            smtp_settings=SmtpSettings(
                server=settings.smtp_server, port=settings.smtp_port,
                username=settings.smtp_username,
                password_file=settings.smtp_password_file,
                ssl=settings.smtp_ssl, tls=settings.smtp_tls),
            to=email_to, subject=message_subject, body=message_body)
        state_db.write_milestone(email_milestone)

    alarm_reasons: Dict[AlarmType, Set[str]] = {}
    for problem in problems:
        alarm_reasons.setdefault(problem.alarm_type, set()).\
            add(problem.alarm_reason)
    state_db.write_alarm_reasons(alarm_reasons)


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = argparse.ArgumentParser(
        description="ULS data download control service healthcheck")
    argument_parser.add_argument(
        "--service_state_db_dsn", metavar="STATE_DB_DSN",
        help=f"Connection string to database containing FS service state "
        f"(that is used by healthcheck script)"
        f"{env_help(Settings, 'service_state_db_dsn')}")
    argument_parser.add_argument(
        "--service_state_db_password_file", metavar="PASWORD_FILE",
        help=f"Optional name of file with password to use in database DSN"
        f"{env_help(Settings, 'service_state_db_password_file')}")
    argument_parser.add_argument(
        "--smtp_server", metavar="SMTP_SERVER",
        help=f"SMTP server. If unspecified no alarm/beacon emails will be "
        f"sent{env_help(Settings, 'smtp_server')}")
    argument_parser.add_argument(
        "--smtp_port", metavar="SMTP_PORT",
        help=f"Port to use on SMTP server{env_help(Settings, 'smtp_port')}")
    argument_parser.add_argument(
        "--smtp_username", metavar="SMTP_USER",
        help=f"Login name for SMTP server. If unspecified no alarm/beacon "
        f"emails will be sent{env_help(Settings, 'smtp_username')}")
    argument_parser.add_argument(
        "--smtp_password_file", metavar="SMTP_PASSWORD_FILE",
        help=f"File with password for SMTP servern (ignored if empty or "
        f"absent){env_help(Settings, 'smtp_password_file')}")
    argument_parser.add_argument(
        "--smtp_ssl", action="store_true",
        help=f"Use SSL with SMTP server{env_help(Settings, 'smtp_ssl')}")
    argument_parser.add_argument(
        "--smtp_tls", action="store_true",
        help=f"Use TLS with SMTP server{env_help(Settings, 'smtp_tls')}")
    argument_parser.add_argument(
        "--email_to", metavar="EMAIL",
        help=f"Email address to send alarms/beacons to. If parameter not "
        f"specified - no alarm/beacon emails will be sent"
        f"{env_help(Settings, 'email_to')}")
    argument_parser.add_argument(
        "--beacon_email_to", metavar="BEACON_EMAIL",
        help=f"Email address to send beacon information  notification to. If "
        f"parameter not specified - alarm email_to will used "
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
    argument_parser.add_argument(
        "--print_email", action="store_true",
        help="Print email instead of sending it (for debug purposes)")

    settings: Settings = \
        cast(Settings, merge_args(settings_class=Settings,
                                  args=argument_parser.parse_args(argv)))

    setup_logging(verbose=settings.verbose)

    set_error_exit_params(log_level=logging.ERROR)
    if settings.force_success:
        set_error_exit_params(exit_code=00)

    state_db = \
        StateDb(db_dsn=settings.service_state_db_dsn,
                db_password_file=settings.service_state_db_password_file)
    try:
        state_db.connect()
    except sa.exc.SQLAlchemyError as ex:
        error(f"Can't connect to FS downloader state database "
              f"{safe_dsn(settings.service_state_db_dsn)}: {repr(ex)}")

    state_db.write_milestone(DownloaderMilestone.Healthcheck)

    email_if_needed(state_db=state_db, settings=settings)

    last_start = state_db.read_milestone(DownloaderMilestone.ServiceStart)
    error_if(last_start is None, "FS downloader service not started")

    error_if(
        expired(event_td=state_db.read_milestone(
                DownloaderMilestone.DownloadStart).get(None),
                max_age_hr=settings.download_attempt_max_age_health_hr),
        f"Last download attempt happened more than "
        f"{settings.download_attempt_max_age_health_hr} hours ago or not at "
        f"all")

    error_if(
        expired(event_td=state_db.read_milestone(
                DownloaderMilestone.DownloadSuccess).get(None),
                max_age_hr=settings.download_success_max_age_health_hr),
        f"Last download success happened more than "
        f"{settings.download_success_max_age_health_hr} hours ago or not at "
        f"all")

    error_if(
        expired(event_td=state_db.read_milestone(
                DownloaderMilestone.DbUpdated).get(None),
                max_age_hr=settings.update_max_age_health_hr),
        f"Last time FS database was updated happened more than "
        f"{settings.update_max_age_health_hr} hours ago or not at all")


if __name__ == "__main__":
    main(sys.argv[1:])
