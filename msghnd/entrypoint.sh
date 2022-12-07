#!/bin/sh

AFC_DEVEL_ENV=${AFC_DEVEL_ENV:=production}
case "$AFC_DEVEL_ENV" in
  "devel")
    echo "Debug profile"
    ;;
  "production")
    echo "Production profile"
    ;;
  *)
    echo "Uknown profile"
    ;;
esac
#
# gunicorn
gunicorn --config gunicorn.conf.py --log-config gunicorn.logs.conf wsgi:app
#
sleep infinity

exit $?
