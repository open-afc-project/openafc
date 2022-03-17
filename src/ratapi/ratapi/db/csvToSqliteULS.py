import csv
import os
import sqlalchemy as sa
from sqlalchemy import Column
from sqlalchemy.sql.elements import and_, or_
from sqlalchemy.sql.expression import tuple_
from sqlalchemy.types import TypeDecorator, String, Boolean, Integer, Float, Unicode, DECIMAL
from numpy import loadtxt
from sqlalchemy.orm import sessionmaker, load_only
import sqlalchemy.ext.declarative as declarative

#: Base class for declarative models
Base = declarative.declarative_base()

class CoerceUTF8(TypeDecorator):
    impl = Unicode

    def process_bind_param(self, value, dialect):
        if isinstance(value, str):
            value = value.decode('utf-8')
        return value

class PR(Base):
    ''' Passive Repeater table
    '''

    __tablename__ = 'pr'

    id = Column(Integer, primary_key=True)

    #: FSID
    fsid = Column(Integer, nullable=False, index=True)

    #: PR IDX
    prSeq = Column(Integer)

    pr_lat_deg = Column(Float)
    pr_lon_deg = Column(Float)
    pr_height_to_center_raat_m = Column(Float)
    pr_reflector_height_m = Column(Float)
    pr_reflector_width_m = Column(Float)
    pr_back_to_back_gain_tx = Column(Float)
    pr_back_to_back_gain_rx = Column(Float)

class ULS(Base):
    ''' ULS Database table
    '''

    __tablename__ = 'uls'
    __table_args__ = (
        sa.UniqueConstraint('fsid'),
    )

    #: FSID
    fsid = Column(Integer, nullable=False, index=True, primary_key=True)

    #: Callsign
    callsign = Column(String(16), nullable=False)

    #: Status
    status = Column(String(1), nullable=False)

    #: Radio Service
    radio_service = Column(String(4), nullable=False)

    #: Entity Name
    name = Column(CoerceUTF8(256), nullable=False)

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

    #: Center Frequency (MHz)
    freq_assigned_start_mhz = Column(Float, nullable=False)
    freq_assigned_end_mhz = Column(Float, nullable=False)

    #: TX EIRP (dBm)
    tx_eirp = Column(Float)

    #: Emissions Designator
    emissions_des = Column(String(64), nullable=False)

    #: Tx Lat Coords
    tx_lat_deg = Column(Float, nullable=False)

    #: Tx Long Coords
    tx_long_deg = Column(Float, nullable=False)

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
    rx_ant_model = Column(CoerceUTF8(64))

    #: Rx Height to Center RAAT (m)
    rx_height_to_center_raat_m = Column(Float)

    #: Rx Gain (dBi)
    rx_gain = Column(Float)

    #: Rx Ant Diameter (m)
    rx_ant_diameter = Column(Float)

    p_rp_num = Column(Integer)

    path_number = Column(Integer, nullable=False)

def _as_bool(s):
    if s == 'Y':
        return True
    if s == 'N':
        return False
    return None


def _as_float(s):
    if s == '' or s is None:
        return None
    return float(s)


def _as_int(s):
    if s == '':
        return None
    return int(s)

def truncate(num, n):
    integer = int(num * (10**n))/(10**n)
    return float(integer)

def load_csv_data(file_name, headers=None):
    ''' Loads csv file into python objects

        :param filename: csv file to load

        :param haders: ((name, type), ...)

        :rtype: (iterable rows, file handle or None)
    '''
    if headers is None:
        file_handle = open(file_name, 'rb')
        return (csv.DictReader(file_handle), file_handle)

    (names, formats) = tuple(zip(*headers))
    return (loadtxt(file_name,
                    skiprows=1,
                    dtype={
                        'names': names,
                        'formats': formats,
                    },
                    delimiter=','), None)

def convertULS(data_file, state_root, logFile):
    logFile.write('Converting ULS csv to sqlite' + '\n')
    fileName = data_file.replace('.csv', '') + '.sqlite3'

    logFile.write("Converting CSV file to SQLITE\n")
    logFile.write("CSV: " + data_file + "\n")
    logFile.write("SQLITE: " + fileName + "\n")

    if os.path.exists(fileName):
        logFile.write("WARNING: sqlite file " + fileName + " already exists, deleting\n")
        os.remove(fileName)
        if os.path.exists(fileName):
            logFile.write("ERROR: unable to delete file " + fileName + "\n")
        else:
            logFile.write("successfully deleted file " + fileName + "\n")

    # the new sqlite for the ULS
    today_engine = sa.create_engine('sqlite:///' + fileName, convert_unicode=True)
    today_session = sessionmaker(bind=today_engine)
    s = today_session()

    # create uls and pr tables
    Base.metadata.create_all(today_engine, tables=[ULS.__table__,PR.__table__])
    try:
        (data, file_handle) = load_csv_data(data_file)

        # generate queries in chunks to reduce memory footprint
        to_save = []
        invalid_rows = 0
        prCount = 0;
        errors = []
        uls = None
        pr = None
        
        for count, row in enumerate(data):
            try:
                numPR = _as_int(row.get("Num Passive Repeater"))
                fsidVal = int(row.get("FSID") or count + 2)
                uls = ULS(
                    #: FSID
                    fsid = fsidVal,
                    #: Callsign
                    callsign=str(row["Callsign"]),
                    #: Status
                    status=str(row["Status"]),
                    #: Radio Service
                    radio_service=str(row["Radio Service"]),
                    #: Entity Name
                    name=str(row["Entity Name"]),
                    #: Mobile
                    mobile=_as_bool(row["Mobile"]),
                    #: Rx Callsign
                    rx_callsign=str(row["Rx Callsign"]),
                    #: Rx Antenna Number
                    rx_antenna_num=int(row["Rx Antenna Number"] or '0'),
                    #: Center Frequency (MHz)
                    freq_assigned_start_mhz=_as_float(row["Lower Band (MHz)"]),
                    freq_assigned_end_mhz=_as_float(row["Upper Band (MHz)"]),
                    #: Tx EIRP (dBm)
                    tx_eirp=_as_float(row["Tx EIRP (dBm)"]),
                    #: Emissions Designator
                    emissions_des=str(row["Emissions Designator"]),
                    #: Tx Lat Coords
                    tx_lat_deg=row["Tx Lat Coords"],
                    #: Tx Long Coords
                    tx_long_deg=row["Tx Long Coords"],
                    #: Tx Ground Elevation (m)
                    tx_ground_elev_m=_as_float(
                            row["Tx Ground Elevation (m)"]),
                    #: Tx Polarization
                    tx_polarization=str(row["Tx Polarization"]),
                    #: Tx Height to Center RAAT (m)
                    tx_height_to_center_raat_m=_as_float(
                            row["Tx Height to Center RAAT (m)"]),
                    #: Tx Gain (dBi)
                    tx_gain=_as_float(row["Tx Gain (dBi)"]),
                    #: Rx Lat Coords
                    rx_lat_deg=row["Rx Lat Coords"],
                    #: Rx Long Coords
                    rx_long_deg=row["Rx Long Coords"],
                    #: Rx Ground Elevation (m)
                    rx_ground_elev_m=_as_float(
                            row["Rx Ground Elevation (m)"]),
                    #: Rx Ant Model
                    rx_ant_model=str(row["Rx Ant Model"]), 
                    #: Rx Height to Center RAAT (m)
                    rx_height_to_center_raat_m=_as_float(
                            row["Rx Height to Center RAAT (m)"]),
                    #: Rx Gain (dBi)
                    rx_gain=_as_float(row["Rx Gain (dBi)"]),
                    #: Rx Ant Diameter (m)
                    rx_ant_diameter=_as_float(row["Rx Ant Diameter (m)"]),
                    #: Number of passive repeaters
                    p_rp_num=numPR,

                    # Path number 
                    path_number=row.get("Path Number")
                )
                s.add(uls)
                for idx in range(1, numPR+1):
                    pr = PR(
                        id = prCount,
                        fsid = fsidVal,
                        prSeq = idx,
                        pr_lat_deg = _as_float(                 row.get("Passive Repeater " + str(idx) + " Lat Coords")),
                        pr_lon_deg = _as_float(                 row.get("Passive Repeater " + str(idx) + " Long Coords")),
                        pr_height_to_center_raat_m = _as_float( row.get("Passive Repeater " + str(idx) + " Height to Center RAAT (m)")),
                        pr_reflector_height_m = _as_float(      row.get("Passive Repeater " + str(idx) + " Reflector Height (m)")),
                        pr_reflector_width_m = _as_float(       row.get("Passive Repeater " + str(idx) + " Reflector Width (m)")),
                        pr_back_to_back_gain_tx = _as_float(    row.get("Passive Repeater " + str(idx) + " Back-to-Back Gain Tx (dBi)")),
                        pr_back_to_back_gain_rx = _as_float(    row.get("Passive Repeater " + str(idx) + " Back-to-Back Gain Rx (dBi)"))
                    )
                    prCount = prCount+1
                    s.add(pr)
                
                # to_save.append(uls)
            except Exception as e:
                logFile.write("ERROR: " + str(e) + '\n')
                invalid_rows = invalid_rows + 1
                if invalid_rows > 50:
                    #errors.append("{}\nData: {}".format(str(e), str(row)))
                    logFile.write('WARN: > 50 invalid rows' + '\n')

            if count % 10000 == 0:
                logFile.write("CSV to sqlite Up to entry " + str(count) + '\n')

        if not (file_handle is None):
            file_handle.close()

        s.commit() # only commit after DB opertation are completed
        logFile.write("File " + str(fileName) + " created with " + str(count) + " entries and " + str(prCount) + " passive repeaters")

        return

    except Exception as e:
        s.rollback()
        raise e
    finally:
        s.close()
