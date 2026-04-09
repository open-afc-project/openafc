#!/bin/sh

if [ -e /wd/private/images ]; then
    mkdir -p /usr/share/fbrat/www/images/
    cp -R /wd/private/images/* /usr/share/fbrat/www/images/
fi

if [ -e /wd/private/templates ]; then
    mkdir -p /usr/lib/python${AFC_PYTHONVERSION}/site-packages/ratapi/templates/
    cp -R /wd/private/templates/*   /usr/lib/python${AFC_PYTHONVERSION}/site-packages/ratapi/templates/
fi

