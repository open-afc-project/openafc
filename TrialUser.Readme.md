# Trial user configuration
AFC has the capability to provide for trial users that have limited ability to perform spectrum availability requests in preset configuration.  This capability has some set up required

## Set up the Trial role 
Run rat-manage-api createdb from inside your rat_server container to create or update your database roles to include the additional Trial role. This only needs to be done once per database installation.

## Set up the Trial configuration

### Create a trial manager user to manage the trial configuration with the AP role [Recommended]
Managing the configuration for the trial user is easier if there is a dedicated user to hold that configuration.  While there can be separate Trial users each with their own log in, all of the spectrum inquiries from all Trial users will share the same configuration which is a configuration managed by a single user..  If the configuration is managed by user that is also running their own inquiries, it would be easy for that user to accidentally update the configuration and throw off all the trial users.

### Set up the trial AP and configuration

Log in as the managing user and create a new AP with the Serial Number **TestSerialNumber** and Certfication ID NRA: **FCC** and id **TestCertificationId**.  If you do not use exactly that Serial Number and Certification ID the trial user's inquiry will not select the correct configuration and may fail.

Then Click on **AFC Config** on the left and set up the configuration as you prefer and save it

### Create a user with the trial role only
Create a new user (or have your user create an account via the UI) and have an admin grant only the Trial role to that user. Users that have Trial and any additional roles will see the capabilities of the additional roles.

After the Trial role has been added, the trial user may log in to site and run inquiries.