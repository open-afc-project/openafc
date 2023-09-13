Copyright © 2022 Broadcom. All rights reserved. The term "Broadcom"
refers solely to the Broadcom Inc. corporate affiliate that owns
the software below.
This work is licensed under the OpenAFC Project License, a copy of which is included with this software program.

# **Introduction**
You can configure AFC server to use one of two login mechanisms.  The OIDC login provides Single Sign On (SSO) where the handling of identity verification is done at the separate identity provider server.  The non OIDC login implements local authentication using the AFC server's local database.

# **Non OIDC Login**
The legacy login mechanism performs authentication locally on the AFC server. It is configured by default.  Steps to configure are described in [README.md](/README.md)

# **OIDC Login**
OIDC relies on an identity provider outside of the AFC server to verify the users identity.  Your organization could already have its own identity server for its employees.  When a user logs in the AFC application, AFC server forwards a request to the identity provider to authenticate.  For your organization's employees, the authentication is completed by your identity provider.  For federated users, the identity provider will further forward the user to his/her respective identity server for verification.

Background on OIDC can be found here: https://openid.net/connect/

## **OIDC Configuration **
### Use Json file
The preferred method is to use an oidc config file in json format, e.g.:
```
{
    "OIDC_LOGIN":"True",
    "OIDC_CLIENT_ID":"1234",
    "OIDC_CLIENT_SECRET":"my_secret_string",
    "OIDC_DISCOVERY_URL":"https://accounts.mycompany.com"
}
```
The path to the config file can be put in a mounted file, passed to the container via environment variable OIDC_ARG. This can be done via docker-compose file, or secrets
For example, docker-compose.yaml:
```
rat_server:
        volumes:
        - /hostpath:/localpath

        environment:
        - OIDC_ARG=/localpath/oidc.json
```
### Use Json file
The alternative method is to use environment variables to pass each parameter, e.g.

For example, docker-compose.yaml:
```
OIDC_LOGIN = True
OIDC_CLIENT_ID = '1234'
OIDC_CLIENT_SECRET = 'my_secret_string'
OIDC_DISCOVERY_URL = 'https://accounts.mycompany.com'
```

Note that the path must be accessible to the httpd which runs under fbrat
```
chown -R 1003:1003 /localpath
```

More information on creating your own google cloud account can be found here:
https://cloud.google.com/apigee/docs/hybrid/v1.3/precog-gcpaccount


## **Migrate From non OIDC to OIDC login method**
With OIDC method, the user acounts are stored in the identity server which maintains accounts of your employees.  Accounts created in non OIDC database are not recognized by OIDC identity server.  Therefore, after converting to OIDC, you will not be able to login via the WEB using test user accounts created via CLI, although those can still be used by test scripts, and the roles of those accounts continue will be maintained.

To facilitate switching real (non test) accounts, when a user logs in for the first time via OIDC, and the email address from the OIDC identity server matches an existing user in the database, that existing account is converted while retaining all roles. Thus, when logging in via WEB GUI, the user has the same access as before the switch.

## **Switching from OIDC to non OIDC login method**
Accounts that are maintained exclusively by OIDC identity provider are not maintained locally, so they cannot be logged in via the WEB GUI unless the admin modify the password. Accounts created via CLI can be logged in using the same password used in the CLI to create them.

In non OIDC, any account's  password can be modified by the admin.  This is true even for accounts that were maintained by OIDC.  However, note that the newly modified password is not recognised by OIDC, and if switched back to OIDC mode again, the password used by the OIDC identity server must be used to login.
