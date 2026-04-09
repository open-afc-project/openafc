"""add fs_uniquifier column to aaa_user for Flask-Security-Too

Revision ID: f1a2b3c4d5e6
Revises: 9fba1618496f
Create Date: 2026-03-30 00:00:00.000000

"""

# revision identifiers, used by Alembic.
import sqlalchemy as sa
from alembic import op
import uuid
revision = 'f1a2b3c4d5e6'
down_revision = '9fba1618496f'


def upgrade():
    op.add_column('aaa_user',
                  sa.Column('fs_uniquifier', sa.String(length=255),
                            nullable=True))

    connection = op.get_bind()
    users = connection.execute(
        sa.text("SELECT id FROM aaa_user WHERE fs_uniquifier IS NULL"))
    for row in users:
        connection.execute(
            sa.text("UPDATE aaa_user SET fs_uniquifier = :uid WHERE id = :id"),
            {"uid": uuid.uuid4().hex, "id": row[0]})

    op.alter_column('aaa_user', 'fs_uniquifier', nullable=False)
    op.create_unique_constraint(
        'uq_aaa_user_fs_uniquifier', 'aaa_user', ['fs_uniquifier'])


def downgrade():
    op.drop_constraint('uq_aaa_user_fs_uniquifier', 'aaa_user',
                       type_='unique')
    op.drop_column('aaa_user', 'fs_uniquifier')
