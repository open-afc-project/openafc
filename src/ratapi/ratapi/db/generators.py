''' Convert files to different formats '''

import logging
import sqlalchemy as sa
from sqlalchemy.orm import sessionmaker
import csv
from numpy import loadtxt, genfromtxt
from .models.base import Base
from .models.population import Population
from .models.uls import ULS

#: LOGGER for this module
LOGGER = logging.getLogger(__name__)


def save_chunk(session, to_save):
    ''' Chunked save operation. Falls back to saving individual enties if sqlalchemy is on old version '''

    if hasattr(session, 'bulk_save_objects'):
        session.bulk_save_objects(to_save)
    else:
        # old version of sqlalchemy don't support bulk save
        LOGGER.warning(
            "SQL alchemy is using old version. This will increase commit time")
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


def create_pop_db(db_name, data_file):
    ''' Creates an sqlite database from a csv file 

        :param db_name: name of sqlite database to be created

        :param data_file: name of csv file to read
    '''

    LOGGER.debug("creating db")
    engine = sa.create_engine('sqlite:///' + db_name + '.sqlite3')

    # drop data from existing table if we are overritting the file
    Base.metadata.drop_all(engine, tables=[Population.__table__])

    # create population table
    Base.metadata.create_all(engine, tables=[Population.__table__])

    LOGGER.debug("creating session")
    session = sessionmaker(bind=engine)
    s = session()

    try:
        LOGGER.debug("loading data file: %s", data_file)
        headers = (
            ('Latitude (deg)', 'f8'),
            ('Longitude (deg)', 'f8'),
            ('Density (people/sq-km)', 'f8'),
        )
        (data, file_handle) = load_csv_data(data_file, headers)

        # generate queries in chunks to reduce memory footprint
        for chunk in range(0, len(data), 10000):
            LOGGER.debug("chunk: %i", chunk)
            print("chunk: %i", chunk)
            save_chunk(s,
                       [
                           Population(
                               latitude=row['Latitude (deg)'],
                               longitude=row['Longitude (deg)'],
                               density=row['Density (people/sq-km)'],
                           )
                           for row in data[chunk:chunk+10000]
                       ]
                       )

        if not (file_handle is None):
            file_handle.close()
        LOGGER.debug("committing")
        s.commit()
    except Exception as e:
        s.rollback()
        LOGGER.error(str(e))
    finally:
        s.close()
        LOGGER.info('Population database created: %s.db', db_name)


def create_uls_db(db_name, data_file):
    ''' create sqlite database from csv file 

        :param db_name: file name of sqlite database to be created

        :param data_file: file name of csv
    '''

    LOGGER.debug("creating db")
    engine = sa.create_engine('sqlite:///' + db_name + '.sqlite3')

    # drop data from existing table if we are overritting the file
    Base.metadata.drop_all(engine, tables=[ULS.__table__])

    # create uls table
    Base.metadata.create_all(engine, tables=[ULS.__table__])

    LOGGER.debug("creating session")
    session = sessionmaker(bind=engine)
    s = session()

    try:
        LOGGER.debug("loading data file: %s", data_file)
        (data, file_handle) = load_csv_data(data_file)

        # generate queries in chunks to reduce memory footprint
        to_save = []
        invalid_rows = 0
        errors = []
        for count, row in enumerate(data):
            try:
                to_save.append(ULS(
                    #: FSID
                    fsid=int(row.get("FSID") or count + 2),
                    #: Callsign
                    callsign=row["Callsign"],
                    #: Status
                    status=row["Status"],
                    #: Radio Service
                    radio_service=row["Radio Service"],
                    #: Entity Name
                    name=row["Entity Name"],
                    #: Mobile
                    mobile=_as_bool(row["Mobile"]),
                    #: Rx Callsign
                    rx_callsign=row["Rx Callsign"],
                    #: Rx Antenna Number
                    rx_antenna_num=int(row["Rx Antenna Number"] or '0'),
                    #: Frequency Assigned (MHz)
                    freq_assigned_start_mhz=_as_float(row["Frequency Assigned (MHz)"].split(
                        "-")[0]) if ("-" in str(row["Frequency Assigned (MHz)"])) else float(row["Frequency Assigned (MHz)"]),
                    freq_assigned_end_mhz=_as_float(row["Frequency Assigned (MHz)"].split(
                        "-")[1]) if ("-" in str(row["Frequency Assigned (MHz)"])) else float(row["Frequency Assigned (MHz)"]),
                    #: Tx EIRP (dBm)
                    tx_eirp=_as_float(row["Tx EIRP (dBm)"]),
                    #: Emissions Designator
                    emissions_des=row["Emissions Designator"],
                    #: Tx Lat Coords
                    tx_lat_deg=_as_float(row["Tx Lat Coords"]),
                    #: Tx Long Coords
                    tx_long_deg=_as_float(row["Tx Long Coords"]),
                    #: Tx Ground Elevation (m)
                    tx_ground_elev_m=_as_float(
                            row["Tx Ground Elevation (m)"]),
                    #: Tx Polarization
                    tx_polarization=row["Tx Polarization"],
                    #: Tx Height to Center RAAT (m)
                    tx_height_to_center_raat_m=_as_float(
                            row["Tx Height to Center RAAT (m)"]),
                    #: Tx Gain (dBi)
                    tx_gain=_as_float(row["Tx Gain (dBi)"]),
                    #: Rx Lat Coords
                    rx_lat_deg=_as_float(row["Rx Lat Coords"]),
                    #: Rx Long Coords
                    rx_long_deg=_as_float(row["Rx Long Coords"]),
                    #: Rx Ground Elevation (m)
                    rx_ground_elev_m=_as_float(
                            row["Rx Ground Elevation (m)"]),
                    #: Rx Ant Model
                    rx_ant_model=row["Rx Ant Model"],
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
                        row.get("Passive Repeater Lat Coords")),
                    #: Passive Repeater Height to Center RAAT (m)
                    p_rp_height_to_center_raat_m=_as_float(
                        row.get("Passive Repeater Height to Center RAAT (m)"))
                ))
            except Exception as e:
                LOGGER.error(str(e))
                invalid_rows = invalid_rows + 1
                if invalid_rows > 50:
                    errors.append("{}\nData: {}".format(str(e), str(row)))

            if count % 10000 == 0:
                print("chunk: %i", count)
                save_chunk(s, to_save)
                to_save = []

        if len(to_save) > 0:
            LOGGER.info("remaining chunk: %i", len(to_save))
            save_chunk(s, to_save)
            to_save = []

        if not (file_handle is None):
            file_handle.close()

        if len(to_save) > 0:
            logging.debug("remaining chunk: %i", len(to_save))
            save_chunk(s, to_save)
            to_save = []

        LOGGER.warn("Invalid rows: %i", invalid_rows)
        LOGGER.debug("committing...")
        s.commit()
        LOGGER.info('ULS database created: %s.sqlite3', db_name)

        return invalid_rows, errors

    except Exception as e:
        s.rollback()
        LOGGER.error(str(e))
        raise e
    finally:
        s.close()


def shp_to_spatialite(dst, src):
    ''' Convert shape file to sql spatialite database 

        NOTE: Does NOT sanitize inputs and runs in shell. Be careful of injection attacks

        :param dst: spatialite file name to write to

        :param src: shp file to read from
    '''
    from subprocess import check_call, CalledProcessError
    import os

    if os.path.exists(dst):
        os.remove(dst)

    try:
        cmd = (
            'ogr2ogr ' +
            '-f SQLite ' +
            '-t_srs "WGS84" ' +
            '-dsco SPATIALITE=YES ' +
            dst + ' ' +
            src
        )
        LOGGER.debug(cmd)
        check_call(cmd, shell=True)
        LOGGER.debug("converted to sqlite.")

    except CalledProcessError as e:
        LOGGER.error(e.cmd)
        LOGGER.error(e.output)
        LOGGER.error(
            "Error raised by shp_to_spatialite with code: %i", e.returncode)
        raise RuntimeError("Shape file could not be converted to database")


def spatialite_to_raster(dst, src, table, elev_field):
    ''' Convert spatialite database into raster file 

        NOTE: Does NOT sanitize inputs and runs in shell. Be careful of injection attacks

        :param dst: raster file to write to

        :param src: spatialite file to read from

        :param table: name of table in spatialite to read from

        :param elev_field: name of field in table to get raster value from
    '''
    from subprocess import check_call, CalledProcessError
    import os

    try:
        check_call(
            'gdal_rasterize ' +
            # use sort to ensure that overlapping polygons give highest hight in raster lookup
            '-sql "SELECT * FROM {} ORDER BY {}" '.format(table, elev_field) +
            # pull from ELEVATION attribute (view in QGIS)
            '-a {} '.format(elev_field) +
            '-of GTiff ' +                                            # output format
            # tile output (optimization)
            '-co "TILED=YES" ' +
            '-ot Float32 ' +                                          # raster data type
            # no data special value
            '-a_nodata 0x7FC00000 ' +
            # resolution (in units of projection)
            '-tr 0.00001 0.00001 ' +
            '-a_srs WGS84 ' +
            src + ' ' +
            dst, shell=True)

    except CalledProcessError as e:
        LOGGER.error(
            "Error raised by shp_to_spatialite with code: %i", e.returncode)
        raise RuntimeError("Spacialite file could not be converted to raster")
