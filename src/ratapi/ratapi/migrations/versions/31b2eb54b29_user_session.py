"""user session

Revision ID: 31b2eb54b29
Revises: 38cf654d18c2
Create Date: 2019-10-18 14:39:49.437802

"""

# revision identifiers, used by Alembic.
revision = '31b2eb54b29'
down_revision = '38cf654d18c2'

from alembic import op
import sqlalchemy as sa


def upgrade():
    ### commands auto generated by Alembic - please adjust! ###
    op.create_table('blacklist_tokens',
    sa.Column('id', sa.Integer(), nullable=False),
    sa.Column('token', sa.String(length=500), nullable=False),
    sa.Column('blacklisted_on', sa.DateTime(), nullable=False),
    sa.PrimaryKeyConstraint('id'),
    sa.UniqueConstraint('token')
    )
    op.create_table('access_point',
    sa.Column('id', sa.Integer(), nullable=False),
    sa.Column('serial_number', sa.String(length=64), nullable=False),
    sa.Column('model', sa.String(length=64), nullable=True),
    sa.Column('manufacturer', sa.String(length=64), nullable=True),
    sa.Column('user_id', sa.Integer(), nullable=True),
    sa.ForeignKeyConstraint(['user_id'], ['aaa_user.id'], ondelete='CASCADE'),
    sa.PrimaryKeyConstraint('id'),
    sa.UniqueConstraint('serial_number')
    )
    op.create_index(op.f('ix_access_point_serial_number'), 'access_point', ['serial_number'], unique=False)
    ### end Alembic commands ###


def downgrade():
    ### commands auto generated by Alembic - please adjust! ###
    op.drop_index(op.f('ix_access_point_serial_number'), table_name='access_point')
    op.drop_table('access_point')
    op.drop_table('blacklist_tokens')
    ### end Alembic commands ###
