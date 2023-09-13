#!/bin/sh

if [ -e /wd/private/images ]; then
    cp -R /wd/private/images/* /usr/share/fbrat/www/images/
fi

if [ -e /wd/private/templates ]; then
    cp -R /wd/private/templates/*   /usr/lib/python3.10/site-packages/ratapi/templates/
fi

