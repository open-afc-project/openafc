''' Package and app definition.
'''

from .app import create_app

if True:
    # Workaround for apache/mod_ssl leaving OpenSSL errors queued
    try:
        from cryptography.hazmat.bindings._openssl import lib as libopenssl
    except ImportError:
        from cryptography.hazmat.bindings._rust import _openssl
        libopenssl = _openssl.lib
    libopenssl.ERR_clear_error()
