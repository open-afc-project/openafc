# Trial user configuration
AFC has the capability to provide for trial users that have limited ability to perform spectrum availability requests in preset configuration.  All of the spectrum inquiries from all Trial users will share the same configuration of the region of the trial AP

### Set up the trial AP and configuration

Log in as the managing user and create a new AP with the Serial Number **TestSerialNumber** and Certfication ID NRA: **FCC** and id **TestCertificationId**.  If you do not use exactly that Serial Number and Certification ID the trial user's inquiry will not select the correct configuration and may fail.

### Create a user with the trial role only
Create a new user (or have your user create an account via the UI) and have an admin grant only the Trial role to that user. Users that have Trial and any additional roles will see the capabilities of the additional roles.

After the Trial role has been added, the trial user may log in to site and run inquiries.
