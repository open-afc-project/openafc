# Test user configuration
Test user can chose **TEST_USA** as a region in the AFC Config tab.  The admin needs to create AP with NRA of **TEST_FCC** for the test user to use in the query.

# New User Registration
New user can request an account on the web page. Filling out the webform to request access to the Broadcom AFC website will send an email to the AFC Admin email alias e.g. afc-admin@broadcom.com.
If a request for access is granted, an email reply informs the user on next steps to get on board.

The configuration in src/ratapi/ratapi/priv_config.py sets the email alias where the registration notification is sent, and the source address of it, and the link for the admin to approve the request. For eg.
```
REGISTRATION_DEST_EMAIL = 'afc-admin@broadcom.com'
REGISTRATION_SRC_EMAIL = 'do_not_reply@afc.broadcom.com'
REGISTRATION_APPROVE_LINK='https:https://approval_url.broadcom.com'
```

Alternative, the values above can be set as environment variables in the rat_server container
