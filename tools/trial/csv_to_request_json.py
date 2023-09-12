import sys
import csv
import json
import os.path 
from enum import Enum

version = '1.4'
cert_rulesetId = 'US_47_CFR_PART_15_SUBPART_E'
cert_id = 'TestCertificationId'
indoor_deployment = 0
global_operating_class = [131, 132, 133, 134, 136, 137]

class LocationType(Enum):
    """ Enum class that defines the location type """
    ELLIPSE = 'Ellipse'
    LINEAR_POLYGON = 'A polygon with specified vertices'
    RADIAL_POLYGON = 'A polygon identified by its center and array of vectors'
    POINT_RADIUS = 'Point/radius'

class ColumnName(Enum):
    TIMESTAMP = 'Timestamp'
    LONTITUDE = 'Longitude of the center of the ellipse in decimal degrees'
    LATITUDE = 'Latitude of the center of the ellipse in decimal degrees '
    HEIGHT = 'Unlicensed device transmit antenna height in meters above ground level (AGL)'
    VERTICAL_UNCERTAINTY = 'Unlicensed device transmit antenna vertical location uncertainty in meters. Must be a positive integer (no decimal point).'
    MAJORAXIS = 'Semi-major axis of the ellipse in meters'
    MINORAXIS = 'Semi-minor axis of the ellipse in meters'
    ORIENTATION = 'Orientation of the major axis of the ellipse, in degrees clockwise from true north'
    POINT_LONTITUDE = 'Unlicensed device transmit antenna longitude in decimal degrees' 
    POINT_LATITUDE = 'Unlicensed device transmit antenna latitude in decimal degrees '
    POINT_RADIUS_AXIS = 'Unlicensed device transmit antenna horizontal location uncertainty radius in meters'
    LINEAR_POLYGON_VERTICES = 'Vertices in decimal degrees'
    CENTER_LONTITUDE = 'Center longitude in decimal degrees'
    CENTER_LATITUDE = 'Center latitude in decimal degrees '
    VECTORS_FROM_CENTER = 'Vectors from Center'
    LOCATION_TYPE = 'Specify the method you choose to provide the horizontal location and horizontal location uncertainty of the unlicensed device.'

def return_int_or_float(str):
    if str == '':
        return ''

    if str.isdigit():
        num = int(str)
    else:
        num = float(str)
    return num

def get_outerboundary_list(str):
    ob_list = []

    ob_str = str.replace(" ", "")
    ob_str = ob_str.strip("[]")
    #print(ob_str)

    ob_split = ob_str.split("),")
    for i in ob_split:
        i = i.strip("()\n\t")
        entry = i.split(",")
        ob_list.append([entry[0], entry[1]])
        #print(i)
    #print(ob_list)
    return ob_list

def generate_request_json(csv_dict, file_index, cert_id):
    data_dict = {}
    asir_list = []
    request_dict = {}
    device_desc_dict = {}
    cert_id_list = []
    location_dict = {}
    freq_list = []
    channel_list = []

    # prepare device descriptor
    timestamp = csv_dict[ColumnName.TIMESTAMP.value].split()
    device_desc_dict['serialNumber'] = 'TestSerialNumber'
    cert_id_list.append(dict(rulesetId=cert_rulesetId, id=cert_id))
    device_desc_dict['certificationId'] = cert_id_list

    # prepare location data
    location_dict['elevation'] = dict(
        height = return_int_or_float(csv_dict[ColumnName.HEIGHT.value]), 
        heightType = 'AGL',
        verticalUncertainty = return_int_or_float(csv_dict[ColumnName.VERTICAL_UNCERTAINTY.value]),
    )

    location_type = csv_dict[ColumnName.LOCATION_TYPE.value]

    if location_type == LocationType.ELLIPSE.value:
        center_dict = dict(
            longitude = return_int_or_float(csv_dict[ColumnName.LONTITUDE.value]), 
            latitude = return_int_or_float(csv_dict[ColumnName.LATITUDE.value]),
        )
        location_dict['ellipse'] = dict(
            center = center_dict,
            majorAxis = return_int_or_float(csv_dict[ColumnName.MAJORAXIS.value]),
            minorAxis = return_int_or_float(csv_dict[ColumnName.MINORAXIS.value]),
            orientation = return_int_or_float(csv_dict[ColumnName.ORIENTATION.value]),
        )
    elif location_type == LocationType.POINT_RADIUS.value:
        center_dict = dict(
            longitude = return_int_or_float(csv_dict[ColumnName.POINT_LONTITUDE.value]), 
            latitude = return_int_or_float(csv_dict[ColumnName.POINT_LATITUDE.value]),
        )
        location_dict['ellipse'] = dict(
            center = center_dict,
            majorAxis = return_int_or_float(csv_dict[ColumnName.POINT_RADIUS_AXIS.value]),
            minorAxis = return_int_or_float(csv_dict[ColumnName.POINT_RADIUS_AXIS.value]),
            orientation = 0
        )
    elif location_type == LocationType.LINEAR_POLYGON.value:
        ob_entry_list = get_outerboundary_list(csv_dict[ColumnName.LINEAR_POLYGON_VERTICES.value])
        ob_list = []
        for i in ob_entry_list:
            ob_list.append(
                dict(longitude=return_int_or_float(i[0]), latitude=return_int_or_float(i[1]))
            )

        location_dict['linearPolygon'] = dict(
            outerBoundary = ob_list,
        )
    elif location_type == LocationType.RADIAL_POLYGON.value:
        center_dict = dict(
            longitude = return_int_or_float(csv_dict[ColumnName.CENTER_LONTITUDE.value]), 
            latitude = return_int_or_float(csv_dict[ColumnName.CENTER_LATITUDE.value]),
        )
        ob_entry_list = get_outerboundary_list(csv_dict[ColumnName.VECTORS_FROM_CENTER.value])
        ob_list = []
        for i in ob_entry_list:
            ob_list.append(
                dict(length=return_int_or_float(i[0]), angle=return_int_or_float(i[1]))
            )
        location_dict['radialPolygon'] = dict(
            center = center_dict,
            outerBoundary = ob_list,
        )
    location_dict['indoorDeployment'] = indoor_deployment

    # prepare frequency
    freq_list.append(dict(lowFrequency=5925, highFrequency=6425))
    freq_list.append(dict(lowFrequency=6525, highFrequency=6875))

    # prepare channel
    for op_class in global_operating_class:
        channel_list.append(dict(globalOperatingClass=op_class))

    request_dict['requestId'] = csv_dict['Unique request ID']
    request_dict['deviceDescriptor'] = device_desc_dict
    request_dict['location'] = location_dict
    request_dict['inquiredFrequencyRange'] = freq_list
    request_dict['inquiredChannels'] = channel_list

    asir_list.append(request_dict)

    data_dict['version'] = version
    data_dict['availableSpectrumInquiryRequests'] = asir_list
    return data_dict

def csv_to_json(csv_file_path, cert_id):
    if os.path.exists(csv_file_path) is False:
        print('input file({}) is not existed'.format(csv_file_path))
        return

    dir = os.path.dirname(csv_file_path)

    if not dir:
        dir = os.getcwd()

    print('Start converting the CSV file to request JSON dir %s => %s' %(csv_file_path, dir))
    with open(csv_file_path, encoding = 'utf-8') as csv_file_handler:
        # convert each row into a dictionary
        csv_reader = csv.DictReader(csv_file_handler)
        i = 2
        for rows in csv_reader:
            file_index = f'UID_' + rows['Unique request ID'] + f'_R{i}'
            i = i+1
            req_json = generate_request_json(rows, file_index, cert_id)

            json_file_path = '{}/{}.json'.format(dir, file_index)
            with open(json_file_path, 'w', encoding = 'utf-8') as json_file_handler:
                json_file_handler.write(json.dumps(req_json, indent = 4))
                print('Converted row {} to {}'.format(file_index, json_file_path))

    print('Finished converting the CSV file to request JSON')

if __name__ == "__main__":
    if len(sys.argv) <= 1:
        sys.exit("Usage: csv_to_request_json.py [csv_file_relative_path]")
    if len(sys.argv) > 2:
        cert_id = sys.argv[2]

    csv_to_json(sys.argv[1], cert_id)    
