''' Package and app definition.
'''

from .app import create_app

if True:
    # Workaround for apache/mod_ssl leaving OpenSSL errors queued
    from cryptography.hazmat.bindings._openssl import lib as libopenssl
    libopenssl.ERR_clear_error()
