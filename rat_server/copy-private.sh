#!/bin/sh

if [ -e /wd/private/images ]; then
    mkdir -p /usr/share/fbrat/www/images/
    cp -R /wd/private/images/* /usr/share/fbrat/www/images/
fi

if [ -e /wd/private/templates ]; then
    mkdir -p /usr/lib/python${AFC_PYTHONVERSION}/site-packages/ratapi/templates/
    cp -R /wd/private/templates/*   /usr/lib/python${AFC_PYTHONVERSION}/site-packages/ratapi/templates/
fi

# Inject private <head> scripts (e.g. cookie consent) into the webpack-built
# index.html.  The template leaves a <!-- PRIVATE_HEAD_SCRIPTS --> placeholder
# that is replaced here with the contents of private/templates/onetrust.html.
if [ -e /wd/private/templates/onetrust.html ]; then
    python3 - <<'PYEOF'
import pathlib
www_index = pathlib.Path('/usr/share/fbrat/www/index.html')
inject = pathlib.Path('/wd/private/templates/onetrust.html').read_text()
www_index.write_text(
    www_index.read_text().replace('<!-- PRIVATE_HEAD_SCRIPTS -->', inject.rstrip()))
PYEOF
fi

