#!/bin/sh

#
# gunicorn
gunicorn --config gunicorn.conf.py --log-config gunicorn.logs.conf wsgi:app
#
sleep infinity

exit $?
