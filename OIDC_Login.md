# AFC Login
## **Introduction**
You can configure AFC server to use one of two login mechanisms.  The OIDC login provides Single Sign On (SSO) where the handling of identity verification is done at the separate identity provider server.  The non OIDC login implements local authentication using the AFC server's local database.

## **Non OIDC Login**
The legacy login mechanism performs authentication locally on the AFC server. It is configured by default.  Steps to configure are described in README.md

## **OIDC Login**
OIDC relies on an identity provider outside of the AFC server to verify the users identity.  Your organization could already have its own identity server for its employees.  When a user logs in the AFC application, AFC server forwards a request to the identity provider to authenticate.  For your organization's employees, the authentication is completed by your identity provider.  For federated users, the identity provider will further forward the user to his/her respective identity server for verification.  

Background on OIDC can be found here: https://openid.net/connect/

### **OIDC Configuration **
Default OIDC configuration is in src/ratapi/ratapi/config.py
By default, it's configured with disable OIDC Login 
```
OIDC_LOGIN = False 
```
OIDC configuration can be customized by creating file src/ratapi/ratapi/priv_config.py. For example, to enable OIDC login, write to priv_config.py:

```
OIDC_LOGIN = True
OIDC_CLIENT_ID = '1234'
OIDC_CLIENT_SECRET = 'my_secret_string'
OIDC_DISCOVERY_URL = 'https://accounts.mycompany.com'
```
To convenience users to test with OIDC, src/ratapi/ratapi/priv_config.py already is populated (commented out) with a functional (non production) Google OIDC cloud account.  This cloud account has been configured to work with an AFC server at a particular URL. Simply uncomment these configuration will enable OIDC and forward traffic there, so that anyone with a valid gmail can use that to login. However, this account is configured with a particular AFC server address so it won't work for any AFC server deployment and should be used only as a template.
 
More information on creating your own google cloud account can be found here:
https://cloud.google.com/apigee/docs/hybrid/v1.3/precog-gcpaccount


### **Local User Creation**
Local users can be created, using CLI just as described in README.md
These users do not have accounts at the identity provider, and therefore, cannot log in via the WEB.  REST API continues to work with users added this way and this mechanism can be used to create test accounts used by regression tests.

```
rat-manage-api user create --role Admin --role AP --role Analysis myusername "Enter Your Password Here"
```
Note: the password in above command has no effect and is kept only to be backward compatible with non OIDC method .  


### **OIDC Authenticated User **
Users authenticated by OIDC identity provider automatically get added to AFC server when they login for the first time, but they are not yet assigned roles.  Roles are assigned as described below.

### **User Roles**
User roles can be given to any user who already has an account. The command to update user's role is:
 
```
rat-manage-api user update --role Admin --role AP --role Analysis --email "user@mycompany.com"
```
Note that for users added via "user create" command, the email and username are the same

Another way to assign roles is via the WEB.  Any user who has admin role and can log in via the WEB can manage other users' roles via the WEB GUI.


## **Migrate From non OIDC to OIDC login method**
With OIDC method, the user acounts are stored in the identity server which maintains accounts of your employees.  Accounts created in non OIDC database are not recognized by OIDC identity server.  Therefore, after converting to OIDC, you will not be able to login via the WEB using test user accounts created via CLI, although those can still be used by test scripts, and the roles of those accounts continue will be maintained.

To make it convenient to switch real (non test) accounts, if an non OIDC account has matching email address in the OIDC identity server, the account will be addopted upon conversion and the user can login via WEB GUI and automatically keeps all the roles (s)he had prior to the switch.

## **Switching from OIDC to non OIDC login method**
Accounts that are maintained exclusively by OIDC identity provider are not maintained locally, so they cannot be logged in via the WEB GUI unless the admin modify the password. Accounts created via CLI can be logged in using the same password used in the CLI to create them.

In non OIDC, any account's  password can be modified by the admin.  This is true even for accounts that were maintained by OIDC.  However, note that the newly modified password is not recognised by OIDC, and if switched back to OIDC mode again, the password used by the OIDC identity server must be used to login.
