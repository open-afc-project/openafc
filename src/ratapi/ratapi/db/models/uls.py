''' Table for ULS database
'''

import sqlalchemy as sa
from sqlalchemy import Column, orm
from sqlalchemy.types import String, Boolean, Integer, Float, Date
from .base import Base


class ULS(Base):
    ''' ULS Database table
    '''

    __tablename__ = 'uls'
    __table_args__ = (
        sa.UniqueConstraint('fsid'),
    )

    #: id (internal)
    id = Column(Integer, nullable=False, primary_key=True)

    #: FSID
    fsid = Column(Integer, nullable=False, index=True)

    #: Callsign
    callsign = Column(String(16), nullable=False)

    #: Status
    status = Column(String(1), nullable=False)

    #: Radio Service
    radio_service = Column(String(4), nullable=False)

    #: Entity Name
    name = Column(String(256), nullable=False)

    #: Common Carrier
    common_carrier = Column(Boolean)

    #: Mobile
    mobile = Column(Boolean)

    #: Rx Callsign
    rx_callsign = Column(String(16), nullable=False)

    #: Rx Location Number
    #rx_location_num = Column(Integer, nullable=False)

    #: Rx Antenna Number
    rx_antenna_num = Column(Integer, nullable=False)

    #: Frequency Assigned (MHz)
    freq_assigned_start_mhz = Column(Float, nullable=False)
    freq_assigned_end_mhz = Column(Float, nullable=False)

    #: TX EIRP (dBm)
    tx_eirp = Column(Float)

    #: Emissions Designator
    emissions_des = Column(String(64), nullable=False)

    #: Tx Lat Coords
    tx_lat_deg = Column(Float, nullable=False, index=True)

    #: Tx Long Coords
    tx_long_deg = Column(Float, nullable=False, index=True)

    #: Tx Ground Elevation (m)
    tx_ground_elev_m = Column(Float)

    #: Tx Polarization
    tx_polarization = Column(String(1), nullable=False)

    #: Tx Height to Center RAAT (m)
    tx_height_to_center_raat_m = Column(Float)

    #: Tx Beamwidth
    #tx_beamwidth = Column(Float, nullable=False)

    #: Tx Gain (dBi)
    tx_gain = Column(Float)

    #: Rx Lat Coords
    rx_lat_deg = Column(Float, nullable=False, index=True)

    #: Rx Long Coords
    rx_long_deg = Column(Float, nullable=False, index=True)

    #: Rx Ground Elevation (m)
    rx_ground_elev_m = Column(Float)

    #: Rx Ant Model
    rx_ant_model = Column(String(64))

    #: Rx Height to Center RAAT (m)
    rx_height_to_center_raat_m = Column(Float)

    #: Rx Gain (dBi)
    rx_gain = Column(Float)

    p_rx_indicator = Column(String(1))

    p_rp_lat_degs = Column(Float, index=True)

    p_rp_lon_degs = Column(Float, index=True)

    p_rp_height_to_center_raat_m = Column(Float)
