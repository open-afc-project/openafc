"""add ap_ip and runtime_opt

Revision ID: cc1c9db1c90d
Revises: 87a874c761f8
Create Date: 2024-11-27 04:52:28.711749

"""
from alembic import op
import sqlalchemy as sa
import sqlalchemy.dialects.postgresql as sa_pg


# revision identifiers, used by Alembic.
revision = 'cc1c9db1c90d'
down_revision = '87a874c761f8'
branch_labels = None
depends_on = None


def upgrade() -> None:
    op.add_column(
        "afc_message", sa.Column("ap_ip", sa_pg.INET(), index=True))
    op.add_column(
        "afc_message", sa.Column("runtime_opt", sa.Integer(), index=True))
    op.create_index(
        "afc_message_rx_envelope_digest_idx", "afc_message",
        ["rx_envelope_digest"])
    op.create_index(
        "afc_message_tx_envelope_digest_idx", "afc_message",
        ["tx_envelope_digest"])
    op.create_index(
        "afc_message_dn_text_digest_idx", "afc_message", ["dn_text_digest"])


def downgrade() -> None:
    op.drop_column("afc_message", "ap_ip")
    op.drop_column("afc_message", "runtime_opt")
    op.drop_index("afc_message_rx_envelope_digest_idx", "afc_message")
    op.drop_index("afc_message_tx_envelope_digest_idx", "afc_message")
    op.drop_index("afc_message_dn_text_digest_idx", "afc_message")
