"""Add MTLS

Revision ID: 4435e833fee6
Revises: 230b7680b81e
Create Date: 2022-10-21 21:10:05.341302

"""

# revision identifiers, used by Alembic.
revision = '4435e833fee6'
down_revision = '230b7680b81e'

from alembic import op
import sqlalchemy as sa


def upgrade():
    ### commands auto generated by Alembic - please adjust! ###
    op.create_table('MTLS',
    sa.Column('id', sa.Integer(), nullable=False),
    sa.Column('cert', sa.String(length=32768), nullable=False),
    sa.Column('note', sa.String(length=128), nullable=True),
    sa.Column('org', sa.String(length=64), nullable=False),
    sa.Column('created', sa.DateTime(), nullable=False),
    sa.PrimaryKeyConstraint('id'),
    )

    op.add_column('aaa_user', sa.Column('org',
                  sa.String(length=50), nullable=True))
    connection = op.get_bind()
    for user in connection.execute('select id, email from aaa_user'):
        if '@' in user[1]:
            org = user[1][user[1].index('@') + 1:]
        else:
            org = ""
        connection.execute(
            "update aaa_user set org = '%s' where id = %d" % (org, user[0]))
    ### end Alembic commands ###


def downgrade():
    ### commands auto generated by Alembic - please adjust! ###
    op.drop_column('aaa_user', 'org')
    op.drop_table('MTLS')
    ### end Alembic commands ###
