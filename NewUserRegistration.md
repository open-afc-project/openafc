# Test user configuration
Test user can chose **TEST_USA** as a region in the AFC Config tab.  The admin needs to create AP with NRA of **TEST_FCC** for the test user to use in the query.

# New User Registration
New user can request an account on the web page. Filling out the webform to request access to the Broadcom AFC website will send an email to the AFC Admin email alias e.g. afc-admin@broadcom.com.
If a request for access is granted, an email reply informs the user on next steps to get on board.

The configuration in src/ratapi/ratapi/priv_config.py sets the email alias where the registration notification is sent, and the source address of it, and the link for the admin to approve the request.

The Mail Server configuration section is optional.  If not, specified, a local server implementation which does not use any encryption, is used.
For eg.
```
# Captcha Config
USE_CAPTCHA=True
CAPTCHA_SECRET='your-captcha-secrets'
CAPTCHA_SITEKEY='your-captcha-sitekey'
CAPTCHA_VERIFY='url-to-verify-captcha'

# Optional Mail Server Configuration
MAIL_SERVER= 'smtp.gmail.com'
MAIL_PORT= 465
MAIL_USE_TLS = False
MAIL_USE_SSL = True
MAIL_USERNAME= 'afc-management-email-address'
MAIL_PASSWORD = "password"

# Mail configuration
REGISTRATION_DEST_EMAIL = 'where-the-registration-email-is-sent' 
REGISTRATION_DEST_PDL_EMAIL = 'group-where-the-registration-email-is-sent'
REGISTRATION_SRC_EMAIL = MAIL_USERNAME
REGISTRATION_APPROVE_LINK='approval link to include in email'
ABOUT_CONTENT='your-private-template.html'

```

Alternative, the values above can be set as environment variables in the rat_server container

Withour your private template, a generic template is presented where the user can enter email,name,and organization.
Once submitted, the afc server will send email to address specified above. SEND_MAIL must be true.  Details of mail server, port, email account, and password needs to be provided.
Additional content of the about page can be put in a private template specified in ABOUT_CONTENT

When USE_CAPTCHA is set, captcha is used on the about page to prevent robots from submitting requests.

The content of the about page can customized in src/ratapi/ratapi/templates/priv_about.html
