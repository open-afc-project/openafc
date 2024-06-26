"""add certification id

Revision ID: 4c904e86218d
Revises: 31b2eb54b29
Create Date: 2020-09-09 14:51:54.624778

"""

# revision identifiers, used by Alembic.
revision = '4c904e86218d'
down_revision = '31b2eb54b29'

from alembic import op
import sqlalchemy as sa


def upgrade():
    ### commands auto generated by Alembic - please adjust! ###
    op.add_column('access_point', sa.Column('certification_id', sa.String(length=64), nullable=True))
    ### end Alembic commands ###


def downgrade():
    ### commands auto generated by Alembic - please adjust! ###
    op.drop_column('access_point', 'certification_id')
    ### end Alembic commands ###
