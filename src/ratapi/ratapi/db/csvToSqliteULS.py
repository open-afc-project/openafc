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
        if isinstance(value, bytes):
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
    pr_height_to_center_raat_tx_m = Column(Float)
    pr_height_to_center_raat_rx_m = Column(Float)
    pr_ant_type = Column(String(3), nullable=False)
    pr_ant_category = Column(String(10))
    pr_ant_model_idx = Column(Integer)
    pr_ant_model = Column(CoerceUTF8(64))
    pr_line_loss = Column(Float)
    pr_reflector_width_m = Column(Float)
    pr_reflector_height_m = Column(Float)
    pr_back_to_back_gain_tx = Column(Float)
    pr_back_to_back_gain_rx = Column(Float)
    pr_ant_diameter_tx = Column(Float)
    pr_ant_diameter_rx = Column(Float)

class ULS(Base):
    ''' ULS Database table
    '''

    __tablename__ = 'uls'
    __table_args__ = (
        sa.UniqueConstraint('fsid'),
    )

    #: FSID
    fsid = Column(Integer, nullable=False, index=True, primary_key=True)

    #: Region
    region = Column(String(16), nullable=False)

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

    #: Tx Lat Coords
    tx_lat_deg = Column(Float)

    #: Tx Long Coords
    tx_long_deg = Column(Float)

    #: Tx Ground Elevation (m)
    tx_ground_elev_m = Column(Float)

    #: Tx Polarization
    tx_polarization = Column(String(1), nullable=False)

    #: Tx Height to Center RAAT (m)
    tx_height_to_center_raat_m = Column(Float)

    #: Tx Architecture (IDU, ODU, UNKNOWN)
    tx_architecture = Column(String(8), nullable=False)

    #Azimuth Angle Towards Tx (deg)
    azimuth_angle_to_tx = Column(Float)

    #Elevation Angle Towards Tx (deg)
    elevation_angle_to_tx = Column(Float)

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

    #: Rx Ant Model Idx
    rx_ant_model_idx = Column(Integer)

    #: Rx Ant Model
    rx_ant_model = Column(CoerceUTF8(64))

    #: Rx Ant Category
    rx_ant_category = Column(CoerceUTF8(64))

    #: Rx Line Loss (dB)
    rx_line_loss = Column(Float)

    #: Rx Height to Center RAAT (m)
    rx_height_to_center_raat_m = Column(Float)

    #: Rx Gain (dBi)
    rx_gain = Column(Float)

    #: Rx Ant Diameter (m)
    rx_ant_diameter = Column(Float)

    #: Rx Near Field Ant Diameter (m)
    rx_near_field_ant_diameter = Column(Float)

    #: Rx Near Field Dist Limit (m)
    rx_near_field_dist_limit = Column(Float)

    #: Rx Near Field Ant Efficiency
    rx_near_field_ant_efficiency = Column(Float)

    #: Rx Diversity Height to Center RAAT (m)
    rx_diversity_height_to_center_raat_m = Column(Float)

    #: Rx Gain (dBi)
    rx_diversity_gain = Column(Float)

    #: Rx Ant Diameter (m)
    rx_diversity_ant_diameter = Column(Float)

    p_rp_num = Column(Integer)

    path_number = Column(Integer, nullable=False)

class RAS(Base):
    ''' ULS Database table
    '''

    __tablename__ = 'ras'

    #: RASID
    rasid = Column(Integer, nullable=False, index=True, primary_key=True)

    region = Column(String(16), nullable=False)
    name = Column(CoerceUTF8(64))
    location = Column(CoerceUTF8(64))
    startFreqMHz = Column(Float)
    stopFreqMHz = Column(Float)
    exclusionZone = Column(String(16))
    rect1lat1 = Column(Float)
    rect1lat2 = Column(Float)
    rect1lon1 = Column(Float)
    rect1lon2 = Column(Float)
    rect2lat1 = Column(Float)
    rect2lat2 = Column(Float)
    rect2lon1 = Column(Float)
    rect2lon2 = Column(Float)
    radiusKm = Column(Float)
    centerLat = Column(Float)
    centerLon = Column(Float)
    heightAGL = Column(Float)

class ANTAOB(Base):
    ''' Antenna Angle Off Boresight
    '''
    __tablename__ = 'antaob'
    aob_idx = Column(Integer, primary_key=True)
    aob_deg = Column(Float)

class ANTNAME(Base):
    ''' Antenna Name
    '''
    __tablename__ = 'antname'
    ant_idx = Column(Integer, primary_key=True)
    ant_name = Column(CoerceUTF8(64))

class ANTGAIN(Base):
    ''' Antenna Gain
    '''
    __tablename__ = 'antgain'
    id = Column(Integer, nullable=False, index=True, primary_key=True)
    gain_db = Column(Float)

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
        file_handle = open(file_name, 'r')
        return (csv.DictReader(file_handle), file_handle)

    (names, formats) = tuple(zip(*headers))
    return (loadtxt(file_name,
                    skiprows=1,
                    dtype={
                        'names': names,
                        'formats': formats,
                    },
                    delimiter=','), None)

def convertULS(fsDataFile, rasDataFile, antennaPatternFile, state_root, logFile, outputSQL):
    logFile.write('Converting ULS csv to sqlite' + '\n')

    logFile.write('Converting CSV file to SQLITE\n')
    logFile.write('FS Data File: ' + fsDataFile + '\n')
    logFile.write('RAS Data File: ' + rasDataFile + '\n')
    logFile.write('Antenna Pattern File: ' + antennaPatternFile + '\n')
    logFile.write('SQLITE: ' + outputSQL + '\n')

    if os.path.exists(outputSQL):
        logFile.write('WARNING: sqlite file ' + outputSQL + ' already exists, deleting\n')
        os.remove(outputSQL)
        if os.path.exists(outputSQL):
            logFile.write('ERROR: unable to delete file ' + outputSQL + '\n')
        else:
            logFile.write('successfully deleted file ' + outputSQL + '\n')

    # the new sqlite for the ULS
    # today_engine = sa.create_engine('sqlite:///' + outputSQL, convert_unicode=True)
    today_engine = sa.create_engine('sqlite:///' + outputSQL)
    today_session = sessionmaker(bind=today_engine)
    s = today_session()

    # create tables: ULS, RAS, PR, ANTAOB, ANTNAME, ANTGAIN
    Base.metadata.create_all(today_engine, tables=[ULS.__table__,PR.__table__,RAS.__table__,ANTAOB.__table__,ANTNAME.__table__,ANTGAIN.__table__])
    try:

        antIdxMap = {}

        ####################################################################################
        # Process antennaPatternFile                                                       #
        ####################################################################################
        (antennaPatternData, file_handle) = load_csv_data(antennaPatternFile)

        antPatternCount = 0
        for fieldIdx, field in enumerate(antennaPatternData.fieldnames):
            if fieldIdx == 0:
                if field != 'Off-axis angle (deg)':
                    sys.exit('ERROR: Invalid antennaPatternFile: ' + antennaPatternFile + ' Especting "Off-axis angle (deg)", found ' + field + '\n')
            else:
                antIdx = fieldIdx-1
                antname = ANTNAME(
                    ant_idx = antIdx,
                    ant_name = field
                )
                s.add(antname)
                antPatternCount += 1
                if field in antIdxMap:
                    sys.exit('ERROR: Invalid antennaPatternFile: ' + antennaPatternFile + ': ' + field + ' defined multiple times\n')
                antIdxMap[field] = antIdx

        antennaPatternFileRows = list(antennaPatternData)
        numAOB = len(antennaPatternFileRows)

        count = 0
        for aobIdx, row in enumerate(antennaPatternFileRows):
            try:
                for fieldIdx, field in enumerate(antennaPatternData.fieldnames):
                    if fieldIdx == 0:
                        antaob = ANTAOB(
                            aob_idx = aobIdx,
                            aob_deg = float(row[field])
                        )
                        s.add(antaob)
                    else:
                        antIdx = fieldIdx-1
                        antgain = ANTGAIN(
                            id = numAOB*antIdx + aobIdx,
                            gain_db = float(row[field])
                        )
                        s.add(antgain)
                        count += 1
                        if (count) % 10000 == 0:
                            logFile.write('CSV to sqlite Up to ANT PATTERN entry ' + str(count) + '\n')

            except Exception as e:
                errMsg = 'ERROR processing antennaPatternFile: ' + str(e) + '\n'
                logFile.write(errMsg)
                sys.exit(errMsg)

        if not (file_handle is None):
            file_handle.close()
        ####################################################################################

        ####################################################################################
        # Process RAS Data                                                                 #
        ####################################################################################
        (rasData, file_handle) = load_csv_data(rasDataFile)

        ras = None

        for count, row in enumerate(rasData):
            try:
                ras = RAS(
                    rasid = int(row['RASID']),

                    region = str(row['Region']),
                    name = str(row['Name']),
                    location = str(row['Location']),
                    startFreqMHz = _as_float(row['Start Freq (MHz)']),
                    stopFreqMHz = _as_float(row['End Freq (MHz)']),
                    exclusionZone = str(row['Exclusion Zone']),
                    rect1lat1 = _as_float(row['Rectangle1 Lat 1']),
                    rect1lat2 = _as_float(row['Rectangle1 Lat 2']),
                    rect1lon1 = _as_float(row['Rectangle1 Lon 1']),
                    rect1lon2 = _as_float(row['Rectangle1 Lon 2']),
                    rect2lat1 = _as_float(row['Rectangle2 Lat 1']),
                    rect2lat2 = _as_float(row['Rectangle2 Lat 2']),
                    rect2lon1 = _as_float(row['Rectangle2 Lon 1']),
                    rect2lon2 = _as_float(row['Rectangle2 Lon 2']),
                    radiusKm = _as_float(row['Circle Radius (km)']),
                    centerLat = _as_float(row['Circle center Lat']),
                    centerLon = _as_float(row['Circle center Lon']),
                    heightAGL = _as_float(row['Antenna AGL height (m)'])
                )
                s.add(ras)
                if (count) % 10000 == 0:
                    logFile.write('CSV to sqlite Up to RAS entry ' + str(count) + '\n')

            except Exception as e:
                errMsg = 'ERROR processing rasDataFile: ' + str(e) + '\n'
                logFile.write(errMsg)
                sys.exit(errMsg)

        if not (file_handle is None):
            file_handle.close()
        ####################################################################################

        ####################################################################################
        # Process FS Data                                                                  #
        ####################################################################################
        (data, file_handle) = load_csv_data(fsDataFile)

        # generate queries in chunks to reduce memory footprint
        to_save = []
        invalid_rows = 0
        prCount = 0;
        errors = []
        uls = None
        pr = None
        antaob = None
        antname = None
        antgain = None

        for count, row in enumerate(data):
            try:
                numPR = _as_int(row.get('Num Passive Repeater'))
                fsidVal = int(row.get('FSID') or count + 2)
                if row['Rx Ant Model Name Matched'] in antIdxMap:
                    rxAntIdx = antIdxMap[row['Rx Ant Model Name Matched']]
                else:
                    rxAntIdx = -1
                uls = ULS(
                    #: FSID
                    fsid = fsidVal,
                    #: Callsign
                    region=str(row['Region']),
                    #: Callsign
                    callsign=str(row['Callsign']),
                    #: Status
                    status=str(row['Status']),
                    #: Radio Service
                    radio_service=str(row['Radio Service']),
                    #: Entity Name
                    name=str(row['Entity Name']),
                    #: Mobile
                    mobile=_as_bool(row['Mobile']),
                    #: Rx Callsign
                    rx_callsign=str(row['Rx Callsign']),
                    #: Rx Antenna Number
                    rx_antenna_num=int(row['Rx Antenna Number'] or '0'),
                    #: Center Frequency (MHz)
                    freq_assigned_start_mhz=_as_float(row['Lower Band (MHz)']),
                    freq_assigned_end_mhz=_as_float(row['Upper Band (MHz)']),
                    #: Tx EIRP (dBm)
                    tx_eirp=_as_float(row['Tx EIRP (dBm)']),
                    #: Tx Lat Coords
                    tx_lat_deg=_as_float(row['Tx Lat Coords']),
                    #: Tx Long Coords
                    tx_long_deg=_as_float(row['Tx Long Coords']),
                    #: Tx Ground Elevation (m)
                    tx_ground_elev_m=_as_float(row['Tx Ground Elevation (m)']),
                    #: Tx Polarization
                    tx_polarization=str(row['Tx Polarization']),
                    #: Tx Height to Center RAAT (m)
                    tx_height_to_center_raat_m=_as_float(row['Tx Height to Center RAAT (m)']),
                    #: Tx Architecture
                    tx_architecture=str(row['Tx Architecture']),
                    # Azimuth Angle Towards Tx (deg)
                    azimuth_angle_to_tx =  _as_float(row['Azimuth Angle Towards Tx (deg)']),
                    # Elevation Angle Towards Tx (deg)
                    elevation_angle_to_tx =  _as_float(row['Elevation Angle Towards Tx (deg)']),
                    #: Tx Gain (dBi)
                    tx_gain=_as_float(row['Tx Gain (dBi)']),
                    #: Rx Lat Coords
                    rx_lat_deg=_as_float(row['Rx Lat Coords']),
                    #: Rx Long Coords
                    rx_long_deg=_as_float(row['Rx Long Coords']),
                    #: Rx Ground Elevation (m)
                    rx_ground_elev_m=_as_float(row['Rx Ground Elevation (m)']),

                    #: Rx Ant Model Idx
                    rx_ant_model_idx=rxAntIdx,

                    #: Rx Ant Model
                    rx_ant_model=str(row['Rx Ant Model Name Matched']),

                    #: Rx Ant Category
                    rx_ant_category=str(row['Rx Ant Category']),
                    #: Rx Line Loss (dB)
                    rx_line_loss=_as_float(row['Rx Line Loss (dB)']),
                    #: Rx Height to Center RAAT (m)
                    rx_height_to_center_raat_m=_as_float(row['Rx Height to Center RAAT (m)']),
                    #: Rx Gain (dBi)
                    rx_gain=_as_float(row['Rx Gain (dBi)']),
                    #: Rx Ant Diameter (m)
                    rx_ant_diameter=_as_float(row['Rx Ant Diameter (m)']),

                    #: Rx Near Field Ant Diameter (m)
                    rx_near_field_ant_diameter = _as_float(row['Rx Near Field Ant Diameter (m)']),
                    #: Rx Near Field Dist Limit (m)
                    rx_near_field_dist_limit = _as_float(row['Rx Near Field Dist Limit (m)']),
                    #: Rx Near Field Ant Efficiency
                    rx_near_field_ant_efficiency = _as_float(row['Rx Near Field Ant Efficiency']),

                    #: Rx Diversity Height to Center RAAT (m)
                    rx_diversity_height_to_center_raat_m=_as_float(row['Rx Diversity Height to Center RAAT (m)']),
                    #: Rx Gain (dBi)
                    rx_diversity_gain=_as_float(row['Rx Diversity Gain (dBi)']),
                    #: Rx Ant Diameter (m)
                    rx_diversity_ant_diameter=_as_float(row['Rx Diversity Ant Diameter (m)']),

                    #: Number of passive repeaters
                    p_rp_num=numPR,

                    # Path number
                    path_number=int(row['Path Number'])
                )

                s.add(uls)
                # print "FSID = " + str(uls.fsid)
                for idx in range(1, numPR+1):
                    if row['Passive Repeater ' + str(idx) + ' Ant Model Name Matched'] in antIdxMap:
                        prAntIdx = antIdxMap[row['Passive Repeater ' + str(idx) + ' Ant Model Name Matched']]
                    else:
                        prAntIdx = -1
                    pr = PR(
                        id = prCount,
                        fsid = fsidVal,
                        prSeq = idx,
                        pr_lat_deg = _as_float(                    row['Passive Repeater ' + str(idx) + ' Lat Coords']),
                        pr_lon_deg = _as_float(                    row['Passive Repeater ' + str(idx) + ' Long Coords']),
                        pr_height_to_center_raat_tx_m = _as_float( row['Passive Repeater ' + str(idx) + ' Height to Center RAAT Tx (m)']),
                        pr_height_to_center_raat_rx_m = _as_float( row['Passive Repeater ' + str(idx) + ' Height to Center RAAT Rx (m)']),
                        pr_ant_type = str(                         row['Passive Repeater ' + str(idx) + ' Ant Type']),
                        pr_ant_category = str(                     row['Passive Repeater ' + str(idx) + ' Ant Category']),
                        pr_ant_model_idx = prAntIdx,
                        pr_ant_model = str(                        row['Passive Repeater ' + str(idx) + ' Ant Model Name Matched']),
                        pr_line_loss = _as_float(                  row['Passive Repeater ' + str(idx) + ' Line Loss (dB)']),
                        pr_reflector_height_m = _as_float(         row['Passive Repeater ' + str(idx) + ' Reflector Height (m)']),
                        pr_reflector_width_m = _as_float(          row['Passive Repeater ' + str(idx) + ' Reflector Width (m)']),
                        pr_back_to_back_gain_tx = _as_float(       row['Passive Repeater ' + str(idx) + ' Back-to-Back Gain Tx (dBi)']),
                        pr_back_to_back_gain_rx = _as_float(       row['Passive Repeater ' + str(idx) + ' Back-to-Back Gain Rx (dBi)']),
                        pr_ant_diameter_tx = _as_float(            row['Passive Repeater ' + str(idx) + ' Tx Ant Diameter (m)']),
                        pr_ant_diameter_rx = _as_float(            row['Passive Repeater ' + str(idx) + ' Rx Ant Diameter (m)'])
                    )
                    prCount = prCount+1
                    s.add(pr)

                # to_save.append(uls)
            except Exception as e:
                logFile.write('ERROR: ' + str(e) + '\n')
                invalid_rows = invalid_rows + 1
                if invalid_rows > 50:
                    #errors.append('{}\nData: {}'.format(str(e), str(row)))
                    logFile.write('WARN: > 50 invalid rows' + '\n')

            if count % 10000 == 0:
                logFile.write('CSV to sqlite Up to FS entry ' + str(count) + '\n')

        if not (file_handle is None):
            file_handle.close()
        ####################################################################################

        s.commit() # only commit after DB opertation are completed
        logFile.write('File ' + str(outputSQL) + ' created with ' + str(count) + ' FS entries, '
             + str(prCount) + ' passive repeaters, '
             + str(antPatternCount) + ' antennaPatterns.\n')

        return

    except Exception as e:
        s.rollback()
        raise e
    finally:
        s.close()
