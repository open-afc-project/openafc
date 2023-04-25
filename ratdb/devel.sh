#!/bin/sh

AFC_DEVEL_ENV=${AFC_DEVEL_ENV:-production}
case "$AFC_DEVEL_ENV" in
  'devel')
    echo -e "\033[0;32mDebug profile\033[0m"
    apk add --no-cache bash
    ;;
  'production')
    echo -e "\033[0;32mProduction profile\033[0m"
    ;;
  *)
    echo "Uknown profile"
    ;;
esac

exit $?
