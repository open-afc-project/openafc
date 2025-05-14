#!/bin/sh

if [ -e /wd/private/images ]; then
    cp -R /wd/private/images/* /usr/share/fbrat/www/images/
fi

if [ -e /wd/private/templates ]; then
    cp -R /wd/private/templates/*   /usr/lib/python${AFC_PYTHONVERSION}/site-packages/ratapi/templates/
fi

