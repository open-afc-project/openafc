"""coordinates converted to PostGIS points

Revision ID: 7a3aa9894e52
Revises: 5663cf8afbd7
Create Date: 2025-02-14 00:40:58.961175

"""
import alembic
from alembic import op
import sqlalchemy as sa
import geoalchemy2 as ga


# revision identifiers, used by Alembic.
revision = '7a3aa9894e52'
down_revision = '5663cf8afbd7'
branch_labels = None
depends_on = None


def upgrade() -> None:
    try:
        op.execute("SELECT ST_AsText(('POINT(0 0)'))")
    except (sa.exc.SQLAlchemyError, alembic.util.exc.CommandError):
        try:
            op.execute("COMMIT")
            op.execute("CREATE EXTENSION IF NOT EXISTS postgis")
            op.execute("BEGIN")
        except (sa.exc.SQLAlchemyError, alembic.util.exc.CommandError) as ex:
            msg = str(ex).replace('\n', ' ')
            raise alembic.util.exc.CommandError(
                f"Can't use PostGIS on \"als\" database and yet postgis "
                f"extension creation failed ({msg}) - maybe due to lack of "
                f"privileges. Please install \"postgis\" extension on \"als\" "
                f"database before proceed")

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
    op.add_column("aps",
                  sa.Column("lat_deg", sa.Float(), index=True, nullable=False,
                            server_default="0.0"))
    op.add_column("aps",
                  sa.Column("lon_deg", sa.Float(), index=True, nullable=False,
                            server_default="0.0"))
    op.execute(
        "UPDATE aps "
        "SET lat_deg=ST_Y(coordinates::geometry), "
        "lon_deg=ST_X(coordinates::geometry)")
    op.drop_column("aps", "coordinates")
