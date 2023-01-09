"""empty message

Revision ID: 41f24c9fbec8
Revises: 20bcbbdc61
Create Date: 2022-12-09 00:14:33.533463

"""

# revision identifiers, used by Alembic.
revision = '41f24c9fbec8'
down_revision = '20bcbbdc61'

from alembic import op
import sqlalchemy as sa
from sqlalchemy.dialects import postgresql

def upgrade():
    ### commands auto generated by Alembic - please adjust! ###
    op.create_table('AFCConfig',
    sa.Column('id', sa.Integer(), nullable=False),
    sa.Column('config', postgresql.JSON(), nullable=True),
    sa.Column('created', sa.DateTime(), nullable=False),
    sa.PrimaryKeyConstraint('id')
    )
    ### end Alembic commands ###


def downgrade():
    ### commands auto generated by Alembic - please adjust! ###
    op.drop_table('AFCConfig')
    ### end Alembic commands ###