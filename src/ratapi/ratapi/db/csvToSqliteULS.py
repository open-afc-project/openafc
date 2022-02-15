import csv
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

class ULS(Base):
    ''' ULS Database table
    '''

    __tablename__ = 'uls'
    __table_args__ = (
        sa.UniqueConstraint('fsid'),
    )

    id = Column(Integer)

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
    rx_lat_deg = Column(Float, nullable=False)

    #: Rx Long Coords
    rx_long_deg = Column(Float, nullable=False)

    #: Rx Ground Elevation (m)
    rx_ground_elev_m = Column(Float)

    #: Rx Ant Model
    rx_ant_model = Column(CoerceUTF8(64))

    #: Rx Height to Center RAAT (m)
    rx_height_to_center_raat_m = Column(Float)

    #: Rx Gain (dBi)
    rx_gain = Column(Float)

    p_rx_indicator = Column(String(1))

    p_rp_lat_degs = Column(Float, index=True)

    p_rp_lon_degs = Column(Float, index=True)

    p_rp_height_to_center_raat_m = Column(Float)

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


def save_chunk(session, to_save):
    ''' Chunked save operation. Falls back to saving individual enties if sqlalchemy is on old version '''
    if hasattr(session, 'bulk_save_objects'):
        session.bulk_save_objects(to_save)
    else:
        for entity in to_save:
            session.add(entity)


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

def lookup_then_insert(session, entries, highestFsId, state_root):
    ''' Looks up the entries in yesterdays DB to retain FSID and inserts into the new DB

        :param session: the sqlalchemy sessions to write to

        :param entries: a list of the ULS entries to be added to the new DB

        :param highestFsId: the largest know FSID to use as a starting point for new entries
    '''
    updateCount = 0
    createCount = 0
    path_to_db = state_root + '/daily_uls_parse/data_files/yesterdayDB.sqlite3'
    # use yesterdays DB to retain FSID
    yesterday_engine = sa.create_engine('sqlite:///' + path_to_db, convert_unicode=True)
    yesterday_session = sessionmaker(bind=yesterday_engine)
    s_yesterday = yesterday_session()
    
    for entry in entries:
        # query yesterday DB for each entry
        row = s_yesterday.query(ULS).filter(and_(ULS.callsign==entry.callsign, ULS.freq_assigned_start_mhz==entry.freq_assigned_start_mhz, ULS.freq_assigned_end_mhz==entry.freq_assigned_end_mhz, ULS.path_number==entry.path_number)).all()
        # if we find an entry that matches, save the FSID but overwrite the entry
        if(len(row) == 1):
            persistentFSID = row[0].fsid
            entry.fsid = persistentFSID 
            updateCount += 1
            session.add(entry)
        elif(len(row) > 1):
            raise Exception("ERROR: Multiple matches found for ULS:", "Callsign: " + str(entry.callsign), "Start Freq: " + str(entry.freq_assigned_start_mhz), "End Freq: " + str(entry.freq_assigned_end_mhz) ,"FSID: " + str(entry.fsid), "Path number: " + str(entry.path_number))
        # otherwise increment fsid counter and store the entry with that FSID
        else:
            highestFsId += 1
            entry.fsid = highestFsId
            createCount += 1
            session.add(entry)
    return [updateCount, createCount, highestFsId]


def convertULS(data_file, state_root, logFile):
    logFile.write('Converting ULS csv to sqlite' + '\n')
    fileName = data_file.replace('.csv', '')
    # the new sqlite for the ULS
    today_engine = sa.create_engine('sqlite:///' + fileName + '.sqlite3', convert_unicode=True)
    today_session = sessionmaker(bind=today_engine)
    s = today_session()

    highestFsId = None
    entriesUpdated = 0
    entriesInserted = 0
    with open('data_files/lastFSID.txt', 'r') as fsidFile: 
        highestFsId = int(fsidFile.read())
    # create uls table
    Base.metadata.create_all(today_engine, tables=[ULS.__table__])
    try:
        (data, file_handle) = load_csv_data(data_file)

        # generate queries in chunks to reduce memory footprint
        to_save = []
        invalid_rows = 0
        errors = []
        uls = None
        
        for count, row in enumerate(data):
            try:
                uls = ULS(
                    id=count,
                    #: FSID
                    fsid=int(row.get("FSID") or count + 2),
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
                    #: Passive Receiver Indicator
                    p_rx_indicator=(
                        row.get("Passive Receiver Indicator") or "N"),
                    #: Passive Repeater Latitude Coords,
                    p_rp_lat_degs=_as_float(
                        row.get("Passive Repeater Lat Coords")),
                    #: Passive Repeater Longitude Coords,
                    p_rp_lon_degs=_as_float(
                        row.get("Passive Repeater Long Coords")),
                    #: Passive Repeater Height to Center RAAT (m)
                    p_rp_height_to_center_raat_m=_as_float(
                        row.get("Passive Repeater Height to Center RAAT (m)")),
                    # Path number 
                    path_number=row.get("Path Number")
                )
                s.add(uls)
                # to_save.append(uls)
            except Exception as e:
                logFile.write("ERROR: " + str(e) + '\n')
                invalid_rows = invalid_rows + 1
                if invalid_rows > 50:
                    #errors.append("{}\nData: {}".format(str(e), str(row)))
                    logFile.write('WARN: > 50 invalid rows' + '\n')

            if count % 10000 == 0:
                logFile.write("CSV to sqlite Up to entry " + str(count) + '\n')
                # result = lookup_then_insert(s, to_save, highestFsId, state_root)
                # entriesUpdated += result[0]
                # entriesInserted += result[1]
                # highestFsId = result[2]
                # to_save = []

        # if len(to_save) > 0:
        #     result = lookup_then_insert(s, to_save, highestFsId, state_root)
        #     entriesUpdated += result[0]
        #     entriesInserted += result[1]
        #     highestFsId = result[2]
        #     to_save = []

        if not (file_handle is None):
            file_handle.close()

        s.commit() # only commit after DB opertation are completed

        # update last assigned fsid file for future
        with open('data_files/lastFSID.txt', 'w') as fsidFile: 
            fsidFile.write(str(highestFsId))
        logFile.write("Updated " + str(entriesUpdated) + ' entries.\nInserted ' + str(entriesInserted) + ' entries.' + '\n')
        return entriesUpdated, entriesInserted

    except Exception as e:
        s.rollback()
        raise e
    finally:
        s.close()


