"""mtls_dn table added

Revision ID: 87a874c761f8
Revises: 4aaf4b7e53f5
Create Date: 2024-10-31 22:55:44.632991

"""
from alembic import op
import sqlalchemy as sa
import sqlalchemy.dialects.postgresql as sa_pg

# revision identifiers, used by Alembic.
revision = '87a874c761f8'
down_revision = '4aaf4b7e53f5'
branch_labels = None
depends_on = None


def upgrade() -> None:
    op.create_table(
        "mtls_dn",
        sa.Column("dn_json", sa_pg.JSONB()),
        sa.Column("dn_text", sa.String(), index=True),
        sa.Column("dn_text_digest", sa_pg.UUID(), index=True),
        sa.Column("month_idx", sa.SmallInteger()))
    op.create_primary_key(
        "mtls_dn_pkey", "mtls_dn", ["dn_text_digest", "month_idx"])
    op.add_column(
        "afc_message", sa.Column("dn_text_digest", sa_pg.UUID(), index=True))
    op.create_foreign_key(
        "afc_message_dn_text_digest_ref", "afc_message", "mtls_dn",
        ["dn_text_digest", "month_idx"], ["dn_text_digest", "month_idx"])


def downgrade() -> None:
    op.drop_constraint("afc_message_dn_text_digest_ref", "afc_message")
    op.drop_column("afc_message", "dn_text_digest")
    op.drop_table("mtls_dn")
