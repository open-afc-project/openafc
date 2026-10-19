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
import pathlib, re
www_index = pathlib.Path('/usr/share/fbrat/www/index.html')
inject = pathlib.Path('/wd/private/templates/onetrust.html').read_text()
content = www_index.read_text()
if '<!-- PRIVATE_HEAD_SCRIPTS -->' in content:
    content = content.replace('<!-- PRIVATE_HEAD_SCRIPTS -->', inject.rstrip())
elif '<!-- OneTrust Cookies Consent Notice start for broadcom.com -->' in content:
    content = re.sub(
        r'<!-- OneTrust Cookies Consent Notice start for broadcom.com -->.*?<!-- OneTrust Cookies Consent Notice end for broadcom.com -->',
        inject.rstrip(),
        content,
        flags=re.DOTALL
    )
www_index.write_text(content)
PYEOF
fi

