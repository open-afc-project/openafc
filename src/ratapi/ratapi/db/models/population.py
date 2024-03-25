''' Table for population density
'''

import sqlalchemy as sa
from sqlalchemy import types
from .base import Base


class Population(Base):
    ''' Population density table
    '''

    __tablename__ = 'population'
    __table_args__ = (
    )

    #: Latitude (deg)
    latitude = sa.Column(
        types.Float,
        sa.CheckConstraint('latitude BETWEEN -90 AND 90'),
        nullable=False,
        primary_key=True,
        index=True)

    #: Longitude (deg)
    longitude = sa.Column(
        types.Float,
        sa.CheckConstraint('longitude BETWEEN -180 AND 180'),
        nullable=False,
        primary_key=True,
        index=True)

    #: Density (people/sq-km)
    density = sa.Column(types.Float, sa.CheckConstraint(
        'density >= 0'), nullable=False)
