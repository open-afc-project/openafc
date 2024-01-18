Copyright © 2022 Broadcom. All rights reserved. The term "Broadcom"
refers solely to the Broadcom Inc. corporate affiliate that owns
the software below.
This work is licensed under the OpenAFC Project License, a copy of which is included with this software program.

# About Page Customization
When a user first enter the webpage without loggin in, the about screen can be accessed on the navigation menu on the left side.  The about screen instructs the user of next steps to gain an account.  About page customization can be done via environment variables or via json file.
## Environment Variables
### Captcha
Captcha configuration is needed if Captcha is to be enabled in the About page to protect the
access request form. 
### Captcha Config

```
USE_CAPTCHA=True
CAPTCHA_SECRET='your-captcha-secrets'
CAPTCHA_SITEKEY='your-captcha-sitekey'
CAPTCHA_VERIFY='url-to-verify-captcha'
```

### Optional Mail Server Configuration
If not, specified, a local server implementation is used, which does not use any encryption.
```
MAIL_SERVER= 'smtp.gmail.com'
MAIL_PORT= 465
MAIL_USE_TLS = False
MAIL_USE_SSL = True
MAIL_USERNAME= 'afc-management-email-address'
MAIL_PASSWORD = "password"
```

### Mail configuration
Mail configuration specifies where email are sent, and what email account is used to send them.  This is used by the AFC server to send notifications to the admin when a new user signs up. 
This is required for a functional About page to handle access requests submitted via the web form.

```
REGISTRATION_DEST_EMAIL = 'where-the-registration-email-is-sent' 
REGISTRATION_DEST_PDL_EMAIL = 'group-where-the-registration-email-is-sent'
REGISTRATION_SRC_EMAIL = MAIL_USERNAME
REGISTRATION_APPROVE_LINK='approval link to include in email'
```

##Json file config
An preferred method other than using environment variables is via json config files.  The json file
is to be put in a volume mounted on the container, and the path must be provided to the server 
via environment variable (RATAPI_ARGR) e.g. using docker-compose environment variable or using secrets.  

e.g. 
Docker compose file:
```
    rat_server:
        environment:
            - RATAPI_ARG=/path/ratapi_config.json
```

The content of the json file is as below:
```
{
    "USE_CAPTCHA":"True",
    "CAPTCHA_SECRET":"somevalue"
    "CAPTCHA_SITEKEY":"somevalue"
    "CAPTCHA_VERIFY":"https://www.google.com/recaptcha/api/siteverify",

    "MAIL_SERVER":"smtp.gmail.com",
    "MAIL_PORT":"465",
    "MAIL_USE_TLS":"False",
    "MAIL_USE_SSL":"True",
    "MAIL_USERNAME":"afc-management-email-address"
    "MAIL_PASSWORD":"password",

    "REGISTRATION_DEST_EMAIL":"where-the-registration-email-is-sent"
    "REGISTRATION_DEST_PDL_EMAIL":"group-where-the-registration-email-is-sent"
    "REGISTRATION_SRC_EMAIL":"afc-management-email-address"
    "REGISTRATION_APPROVE_LINK":"approval link to include in email"
}
```
Note that the path must be accessible to the httpd which runs under fbrat
```
chown -R 1003:1003 /localpath
```


# OIDC Configuration
See OIDC_Login.md

# Miscellaneous Configurations
## private file
Under root of the source code (at same level as src), you can create private directory.
The structure:
private/
    templates/
    images/
Under templates: various templates that can be used to customize various web form, for eg.
    about.html: This is the page the user can access first to sign up as a new user.  This can be customized to give more detail sign up instruction
    flask_user_layout.html: to customize the login page for the non-OIDC method.
Under images: the files here customize various images of the web page.
    logo.png: the company logo on the Information (i) page
    background.png: the background image on the Information (i) page 

## Company Name:
   The config json file (RATAPI_ARG) accepts entry for "USER_APP_NAME" to customize the company name that appears in
   various parts of the webpage. e.g.
   {
     ... snip ...
     "USER_APP_NAME":"My company AFC"
   }
