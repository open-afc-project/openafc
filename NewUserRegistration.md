Copyright © 2022 Broadcom. All rights reserved. The term "Broadcom"
refers solely to the Broadcom Inc. corporate affiliate that owns
the software below.
This work is licensed under the OpenAFC Project License, a copy of which is included with this software program.

# New User Creation
New users can be created via the CLI or can be registered via the Web GUI

## CLI to create users
```
rat-manage-api user create --role Admin --role AP --role Analysis myusername "Enter Your Password Here"
```
## CLI to assign roles.
Roles for existing users can be modified using CLI
Note that for users added via "user create" command, the email and username are the same
```
rat-manage-api user update --role Admin --role AP --role Analysis --email "user@mycompany.com"
```

# Web User Registration
New user can request an account on the web page.  For non-OIDC method, use the register button and follow the instructions sent via email.
For OIDC signin method, use the About link to fill out the request form.  If a request for access is granted, an email reply informs the user on next steps to get on board.  Custom configuration is required on the server to handle new user requests. See Customization.md for details.
Regardless of method newly granted users will have default Trial roles upon first time the person logs in.  The Admin or Super user can use the web GUI to change a user roles

# New User access
New Users who are granted access automatically have Trial roles and have access to the Virtual AP tab to submit requests.  
Test user can chose from **TEST_US** **TEST_CA** or **TEST_BR** in the drop down in the Virtual AP tab.  The corresponding config for these test regsions should first be set in AFC Config tab by the admin. New Users who are granted access automatically have Trial roles and have access to the Virtual AP tab to submit requests.


