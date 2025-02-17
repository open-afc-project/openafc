"""coordinates converted to PostGIS points

Revision ID: 7a3aa9894e52
Revises: 5663cf8afbd7
Create Date: 2025-02-14 00:40:58.961175

"""
from alembic import op
import sqlalchemy as sa
import geoalchemy2 as ga


# revision identifiers, used by Alembic.
revision = '7a3aa9894e52'
down_revision = '5663cf8afbd7'
branch_labels = None
depends_on = None


def upgrade() -> None:
    op.execute("CREATE EXTENSION IF NOT EXISTS postgis")
    op.add_column(
        "aps",
        sa.Column("coordinates",
                  ga.Geography(geometry_type="POINT", srid=4326), index=True,
                  nullable=False,
                  server_default=sa.text(
                      "ST_GeographyFromText('SRID=4326;POINT(0 0)')")))
    op.execute(
        "UPDATE aps "
        "SET coordinates=ST_SetSRID(ST_MakePoint(lon_deg, lat_deg), 4326)")
    op.drop_column("aps", "lat_deg")
    op.drop_column("aps", "lon_deg")


def downgrade() -> None:
    op.drop_column("aps", "coordinates")
    op.execute("DELETE FROM aps")
