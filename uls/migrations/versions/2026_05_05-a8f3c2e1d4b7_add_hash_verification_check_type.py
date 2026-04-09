"""Add HashVerification to CheckType enum

Revision ID: a8f3c2e1d4b7
Revises: 6926c125ef61
Create Date: 2026-05-05 00:00:00.000000

"""
from alembic import op
import sqlalchemy as sa

revision = 'a8f3c2e1d4b7'
down_revision = '6926c125ef61'
branch_labels = None
depends_on = None


def upgrade() -> None:
    # PostgreSQL requires explicit ALTER TYPE to add an enum value.
    # IF NOT EXISTS is supported from PostgreSQL 9.6 onwards; it makes
    # repeated runs (e.g. after a failed migration) idempotent.
    op.execute(
        sa.text("ALTER TYPE checktype ADD VALUE IF NOT EXISTS 'HashVerification'"))


def downgrade() -> None:
    # PostgreSQL does not support removing enum values without recreating the
    # type. Since the value is harmlessly unused after downgrade, we leave it
    # in place rather than a destructive table drop/recreate cycle.
    pass
