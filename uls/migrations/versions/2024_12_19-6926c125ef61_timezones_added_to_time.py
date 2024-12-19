"""Timezones added to time

Revision ID: 6926c125ef61
Revises: b41cf8a00430
Create Date: 2024-12-19 06:49:20.298546

"""
from alembic import op
import sqlalchemy as sa
import enum

# revision identifiers, used by Alembic.
revision = '6926c125ef61'
down_revision = 'b41cf8a00430'
branch_labels = None
depends_on = None

DownloaderMilestone = \
    enum.Enum("DownloaderMilestone",
              [
                  # Service birth (first write to milestone database)
                  "ServiceBirth",
                  # Service start
                  "ServiceStart",
                  # Download start
                  "DownloadStart",
                  # Download success
                  "DownloadSuccess",
                  # FS region changed
                  "RegionChanged",
                  # FS database file updated
                  "DbUpdated",
                  # External parameters checked
                  "ExtParamsChecked",
                  # Healthcheck performed
                  "Healthcheck",
                  # Beacon sent
                  "BeaconSent",
                  # Alarm sent
                  "AlarmSent"])

# Type of log
LogType = \
    enum.Enum("LogType",
              [
                  # Log of last download attempt
                  "Last",
                  # Log of last failed attempt
                  "LastFailed",
                  # Log of last completed update
                  "LastCompleted"])

# Check type
CheckType = enum.Enum("CheckType", ["ExtParams", "FsDatabase"])

# Alarm types
AlarmType = enum.Enum("AlarmType", ["MissingMilestone", "FailedCheck"])


def recreate_tables(with_timezone: bool) -> None:
    """ Drop and recreate tables with/without timezonws in timestamps """
    op.drop_table("milestones")
    op.drop_table("alarms")
    op.drop_table("logs")
    op.drop_table("checks")

    op.execute(sa.text("DROP TYPE downloadermilestone"))
    op.execute(sa.text("DROP TYPE alarmtype"))
    op.execute(sa.text("DROP TYPE logtype"))
    op.execute(sa.text("DROP TYPE checktype"))

    op.create_table(
        "milestones",
        sa.Column("milestone", sa.Enum(DownloaderMilestone), index=True,
                  primary_key=True),
        sa.Column("region", sa.String(), index=True, primary_key=True),
        sa.Column("timestamp", sa.DateTime(timezone=with_timezone),
                  nullable=False))
    op.create_table(
        "alarms",
        sa.Column("alarm_type", sa.Enum(AlarmType), primary_key=True,
                  index=True),
        sa.Column("alarm_reason", sa.String(), primary_key=True,
                  index=True),
        sa.Column("timestamp", sa.DateTime(timezone=with_timezone),
                  nullable=False))
    op.create_table(
        "logs",
        sa.Column("log_type", sa.Enum(LogType), index=True, nullable=False,
                  primary_key=True),
        sa.Column("text", sa.String(), nullable=False),
        sa.Column("timestamp", sa.DateTime(timezone=with_timezone),
                  nullable=False, index=True))
    op.create_table(
        "checks",
        sa.Column("check_type", sa.Enum(CheckType), primary_key=True,
                  index=True),
        sa.Column("check_item", sa.String(), primary_key=True, index=True),
        sa.Column("errmsg", sa.String(), nullable=True),
        sa.Column("timestamp", sa.DateTime(timezone=with_timezone),
                  nullable=False))


def upgrade() -> None:
    """ Recreate all tables to timezoned """
    recreate_tables(with_timezone=True)        

def downgrade() -> None:
    """ Recreate all tables to nontimezoned """
    recreate_tables(with_timezone=True)        
