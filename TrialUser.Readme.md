# Trial user configuration
AFC has the capability to provide for trial users that have limited ability to perform spectrum availability requests in preset configuration.

### Create a user with the trial role only
The new user can register for account online via the UI.  Upon approval, the user is granted by default the Trial role, and can run start sending inquiries.

### Running the Spectrum query as the Trial user
The trial user can simply provide **TestCertificationId** as the certification ID and **TestSerialNumber** as the serial number in the Spectrum query.

> **WARNING — Development/test environments only**
>
> The identifiers `TestCertificationId` / `TestSerialNumber` (and the related
> `HeatMapCertificationId` / `HeatMapSerialNumber`) are accepted **only when the
> server-side environment variable `AFC_ENABLE_TEST_CERTS=true` is set**.
>
> **`AFC_ENABLE_TEST_CERTS` MUST NOT be set to `true` in a production
> deployment.** When enabled, device-certification enforcement is disabled for
> these well-known identifiers. This setting is intended only for local
> development and automated CI testing; it must never be active in production.
>
> If `AFC_ENABLE_TEST_CERTS=true` is detected at startup, `afcserver` and
> `rat_server` log a **CRITICAL** security warning.
>
> For trial users in a production environment, register their AP configurations
> through the Admin UI (device certification database) and assign them the
> `Trial` role through normal account management. Do **not** rely on
> `TestCertificationId` outside of isolated development environments.

