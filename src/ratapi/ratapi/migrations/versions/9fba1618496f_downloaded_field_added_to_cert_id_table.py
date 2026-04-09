"""'downloaded' field added to cert_id table

Revision ID: 9fba1618496f
Revises: 0574ac12b90c
Create Date: 2025-04-17 06:31:45.747673

"""

# revision identifiers, used by Alembic.
import sqlalchemy as sa
from alembic import op
revision = '9fba1618496f'
down_revision = '0574ac12b90c'


def upgrade():
    op.add_column(
        'cert_id',
        sa.Column('downloaded', sa.Boolean(), nullable=False,
                  server_default=sa.sql.expression.false()))


def downgrade():
    op.drop_column('cert_id', 'downloaded')
