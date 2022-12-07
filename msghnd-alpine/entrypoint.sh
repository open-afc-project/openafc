#!/bin/sh

AFC_DEVEL_ENV=${AFC_DEVEL_ENV:=production}
case "$AFC_DEVEL_ENV" in
  "devel")
    echo "Debug proile"
    ;;
  "production")
    echo "Production proile"
    ;;
  *)
    echo "Uknown proile"
    ;;
esac
#
# gunicorn
gunicorn --config gunicorn.conf.py --log-config gunicorn.logs.conf wsgi:app
#
sleep infinity

exit $?
