''' API related to IETF RFC 7545 "Protocol to Access White-Space Databases".
All of the API uses JSON-RPC transport on a single endpoint URI.
'''

import logging
import datetime
import subprocess
import os
import json
import shutil
from random import gauss, random
import threading
from flask_jsonrpc import JSONRPC
from flask_jsonrpc.exceptions import (
    InvalidParamsError, ServerError)
import flask
from ..util import TemporaryDirectory, getQueueDirectory
from ..xml_utils import (datetime_to_xml)
from afcmodels.aaa import User
import gzip
from .ratapi import build_task

RULESET = 'AFC-6GHZ-DEMO-1.1'

#: Logger for this module
LOGGER = logging.getLogger(__name__)
LOCK = threading.Lock()

# thread safe generation of brown noise array


def brownNoise(start_mu, sigma, length):
    ''' Thread safe generation of a brown noise array
    '''
    LOGGER.debug("Waiting for random number lock")
    LOCK.acquire()
    arr = [start_mu]
    prev_val = start_mu
    for _i in range(length-1):
        if False:  # random() < 0.25:
            arr.append(None)
        else:
            arr.append(gauss(prev_val, sigma))
            prev_val = arr[-1]

    LOGGER.debug("Releasing random number lock")
    LOCK.release()
    return arr


def genProfiles(bandwith, start_freq, num_channels, start_mu, sigma):
    ''' Create properly formatted list of profiles for PAWS
    returns { dbm: number, hz: number }[][]
    '''
    values = brownNoise(start_mu, sigma, num_channels)
    profiles = []
    current_profile = []
    begin_freq = start_freq
    end_freq = begin_freq + bandwith
    for dbm in values:
        if not (dbm is None):
            current_profile.append(dict(dbm=dbm, hz=begin_freq))
            current_profile.append(dict(dbm=dbm, hz=end_freq))
        elif len(current_profile) > 0:
            profiles.append(current_profile)
            current_profile = []

        begin_freq = end_freq
        end_freq = begin_freq + bandwith

    if len(current_profile) > 0:
        profiles.append(current_profile)
    return profiles


def _auth_paws(serial_number, model, manufacturer, rulesets):
    ''' Authenticate an access point. If must match the serial_number, model, and manufacturer in the database to be valid '''
    if serial_number is None:
        raise InvalidParamsError('serialNumber is required in the deviceDesc')

    if rulesets is None or len(rulesets) != 1 or rulesets[0] != RULESET:
        raise InvalidParamsError(
            'Invalid rulesetIds: ["{}"] expected'.format(RULESET))


def getSpectrum(**kwargs):
    ''' Analyze spectrum availability for a single RLAN AP.
    Parameters are AVAIL_SPECTRUM_REQ data and
    result is AVAIL_SPECTRUM_RESP data.
    '''


    from flask_login import current_user

    REQUIRED_PARAMS = (
        'deviceDesc',
        'location',
        'antenna',
        'capabilities',
        'type',
        'version',
    )

    missing_params = frozenset(REQUIRED_PARAMS) - frozenset(list(kwargs.keys()))
    if missing_params:
        raise InvalidParamsError(
            'Required parameter names: {0}'.format(' '.join(REQUIRED_PARAMS)))

    now_time = datetime.datetime.utcnow()
    spectrum_specs = []

    if flask.current_app.config["PAWS_RANDOM"]:
        # this is sample data generation. remove once afc-engine is working
        spectrum1 = dict(
            resolutionBwHz=20e6,
            profiles=genProfiles(
                bandwith=20e6,
                start_freq=5935e6,
                num_channels=59,
                start_mu=40,
                sigma=5),
        )
        spectrum2 = dict(
            resolutionBwHz=40e6,
            profiles=genProfiles(
                bandwith=40e6,
                start_freq=5935e6,
                num_channels=29,
                start_mu=30,
                sigma=7),
        )
        spectrum3 = dict(
            resolutionBwHz=80e6,
            profiles=genProfiles(
                bandwith=80e6,
                start_freq=5935e6,
                num_channels=14,
                start_mu=35,
                sigma=10),
        )
        spectrum4 = dict(
            resolutionBwHz=160e6,
            profiles=genProfiles(
                bandwith=160e6,
                start_freq=5935e6,
                num_channels=7,
                start_mu=28,
                sigma=12),
        )
        spectrum_specs.append(dict(
            rulesetInfo=dict(
                authority='US',
                rulesetId='AFC-6GHZ-DEMO-1.1',
            ),
            spectrumSchedules=[
                dict(
                    eventTime=dict(
                        startTime=datetime_to_xml(now_time),
                        stopTime=datetime_to_xml(
                            now_time + datetime.timedelta(days=1)),
                    ),
                    spectra=[
                        spectrum1,
                        spectrum2,
                        spectrum3,
                        spectrum4,
                    ],
                ),
            ],
        ))

        LOGGER.debug("Returning sample data")
        return dict(
            type="AVAIL_SPECTRUM_RESP",
            version="1.0",
            timestamp=datetime_to_xml(now_time),
            deviceDesc=kwargs['deviceDesc'],
            spectrumSpecs=spectrum_specs,
        )

    # authenitcate access point
    description = kwargs['deviceDesc']
    auth_paws(description.get('serialNumber'), description.get(
-        'modelId'), description.get('manufacturerId'), description.get('rulesetIds'))
    user_id = current_user.id
    user = User.query.filter_by(id=user_id).first()

    request_type = "APAnalysis"
    temp_dir = getQueueDirectory(
        flask.current_app.config['TASK_QUEUE'], request_type)

    response_file_path = os.path.join(
        temp_dir, 'pawsResponse.json')

    request_file_path = os.path.join(temp_dir, 'analysisRequest.json')

    LOGGER.debug("Writing request file: %s", request_file_path)
    with open(request_file_path, "w") as f:
        f.write(flask.jsonify(kwargs).get_data(as_text=True))
    LOGGER.debug("Request file written")

    task = build_task(request_file_path,response_file_path,request_type,user_id,user,temp_dir)

    if task.state == 'FAILURE':
        raise ServerError('Task was unable to be started')

    LOGGER.debug("WAITING for task to complete")
    # wait for task to complete and get result
    task.wait()

    if task.failed():
        raise ServerError('Task excecution failed')

    if task.successful() and task.result['status'] == 'DONE':
        if not os.path.exists(task.result['result_path']):
            raise ServerError('Resource already deleted')
        # read temporary file generated by afc-engine
        LOGGER.debug("Reading result file: %s", task.result['result_path'])
        with gzip.open(task.result['result_path'], 'rb') as resp_file:
            return json.loads(resp_file.read())

    elif task.successful() and task.result['status'] == 'ERROR':
        if not os.path.exists(task.result['error_path']):
            raise ServerError('Resource already deleted')
        # read temporary file generated by afc-engine
        LOGGER.debug("Reading error file: %s", task.result['error_path'])
        with open(task.result['error_path'], 'rb') as error_file:
            raise ServerError(error_file.read(), task.result['exit_code'])

    else:
        raise ServerError('Invalid task state')


def create_handler(app, path):
    ''' The PAWS interface is all JSON-RPC over a single endpoint path.

    :param app: The flask application to bind to.

    :param path: The root path to bind under.

    :return: The :py:cls:`JSONRPC` object created.
    '''
    rpc = JSONRPC(service_url=path, enable_web_browsable_api=True)
    rpc.method('spectrum.paws.getSpectrum(deviceDesc=object, location=object, antenna=object, capabilities=object, type=str, version=str) -> object', validate=False)(getSpectrum)
    rpc.init_app(app)

    return rpc
