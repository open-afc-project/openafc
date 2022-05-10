#!/usr/bin/env python

# Copyright 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

"""
Provides HTTP server for file exchange between Celery clients and workers.
"""

import os
from flask import Flask, request, helpers, abort, make_response
import logging
from werkzeug.utils import secure_filename

flask = Flask(__name__)
flask.config.from_pyfile('filestorage_config.py')

if flask.config['LOG_STREAM']:
    logging.basicConfig(stream=flask.config['LOG_STREAM'],
                        level=flask.config['LOG_LEVEL'])
elif flask.config['LOG_FILE']:
    logging.basicConfig(filename=flask.config['LOG_FILE'],
                        level=flask.config['LOG_LEVEL'])
else:
    logging.basicConfig(level=flask.config['LOG_LEVEL'])


@flask.route('/'+flask.config['FSVERSION']+'<path>', methods=["DELETE"])
def fffdelete(path):
    ''' File delete handler. Receives hostname://filename '''
    flask.logger.debug('method={}, path={}'.format(request.method, path))
    local_path = os.path.join(flask.config['FILE_LOCATION'],
                              secure_filename(path))
    if not os.path.exists(local_path):
        flask.logger.error('{}: File not found'.format(local_path))
        abort(404, 'File not found.')
    flask.logger.debug('DELETE {}'.format(local_path))
    os.remove(local_path)
    return make_response('OK', 204)


@flask.route('/'+flask.config['FSVERSION']+'<path>', methods=['POST'])
def fffpost_path(path):
    ''' File upload handler. Receives hostname://filename
    It handles for requests.post(data=...) '''
    flask.logger.debug('method={}, path={}'.format(request.method, path))
    local_path = os.path.join(flask.config['FILE_LOCATION'],
                              secure_filename(path))
    with open(local_path, 'wb') as f:
        f.write(request.data)
    return make_response('OK', 200)


@flask.route('/'+flask.config['FSVERSION'], defaults={'path': ''}, methods=['POST'])
def fffpost_request(path):
    ''' File upload handler. Receives url hostname only,
    filename should be in request.files['file'].filename
    It handles requests.post(files=...) or curl '''
    flask.logger.debug('method={}, path={}'.format(request.method, path))
    if 'file' not in request.files:
        flask.logger.error('No file in request')
        abort(422, 'Unprocessable Entity')
    file = request.files['file']
    if not file:
        flask.logger.error('No file in request')
        abort(422, 'Unprocessable Entity')
        # return redirect(request.url)
    if file.filename == '':
        flask.logger.error('Empty filename')
        abort(422, 'Unprocessable Entity')
        # return redirect(request.url)
    local_path = os.path.join(flask.config['FILE_LOCATION'],
                              secure_filename(request.files['file'].filename))
    flask.logger.debug('POST {}'.format(local_path))
    request.files['file'].save(local_path)
    return make_response('OK', 200)


@flask.route('/'+flask.config['FSVERSION']+'<path:path>', methods=['GET'])  # handle URL with filename
def fffget(path):
    ''' File download handler. Receives hostname://filename '''
    flask.logger.debug('method={}, path={}'.format(request.method, path))
    local_path = os.path.join(flask.config['FILE_LOCATION'],
                              secure_filename(path))
    if not os.path.exists(local_path):
        flask.logger.error('{}: File not found'.format(local_path))
        abort(404, 'File not found.')
    flask.logger.debug('GET {}'.format(local_path))
    return helpers.send_file(local_path)


@flask.route('/'+flask.config['FSVERSION']+'<path:path>', methods=['HEAD'])  # handle URL with filename
def fffhead(path):
    ''' Is file exist handler. Receives hostname://filename '''
    flask.logger.debug('method={}, path={}'.format(request.method, path))
    local_path = os.path.join(flask.config['FILE_LOCATION'],
                              secure_filename(path))
    if not os.path.exists(local_path):
        abort(404, 'File not found.')
    return make_response('OK', 200)


if __name__ == '__main__':
    # In Centos7 waitress module is missing, so "yum install python2-waitress"
    # production env:
    from waitress import serve
    flask.logger.debug("host={} port={}".format(flask.config['SERVER_HOST'], flask.config['SERVER_PORT']))
    serve(flask, host=flask.config['SERVER_HOST'],
          port=flask.config['SERVER_PORT'])
    # Development env:
    # flask.run(host=flask.config['SERVER_HOST'],
    #            port=flask.config['SERVER_PORT'], debug=True)

# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
