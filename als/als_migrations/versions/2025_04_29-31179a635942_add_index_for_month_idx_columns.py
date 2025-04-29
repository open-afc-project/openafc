"""Add index for month_idx columns

Revision ID: 31179a635942
Revises: cc1c9db1c90d
Create Date: 2025-04-29 07:43:29.001027

"""
from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision = '31179a635942'
down_revision = 'cc1c9db1c90d'
branch_labels = None
depends_on = None

TABLES = [
    "afc_message",
    "rx_envelope",
    "tx_envelope",
    "mtls_dn",
    "request_response_in_message",
    "request_response",
    "device_descriptor",
    "certification",
    "compressed_json",
    "customer",
    "location",
    "afc_config",
    "geo_data_version",
    "uls_data_version",
    "max_psd",
    "max_eirp",
    "afc_server",
    "decode_error"]


def upgrade() -> None:
    """ Add index on 'month_idx' column to all tables """
    for table in TABLES:
        op.create_index(index_name=f"{table}_month_idx_idx",
                        table_name=table, columns=["month_idx"],
                        if_not_exists=True)


def downgrade() -> None:
    """ Remove index on 'month_idx' column from all tables """
    for table in TABLES:
        op.drop_index(index_name=f"{table}_month_idx_idx", table_name=table,
                      if_exists=True)
