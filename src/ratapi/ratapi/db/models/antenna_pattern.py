''' Table for antenna patterns
'''

import sqlalchemy as sa
from sqlalchemy import types
from base import Base


class Population(Base):
    ''' Antenna Pattern table
    '''

    __tablename__ = 'antenna_pattern'
    __table_args__ = (
    )

    #: id
    id = sa.Column(types.Integer, nullable=False, primary_key=True)

    #: Model
    model = sa.Column(types.String(64), nullable=False, index=True)

    #: Pattern (stored as raw array of 1800 32-bit floats)
    pattern = sa.Column(types.LargeBinary, nullable=False)
