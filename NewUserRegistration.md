Copyright © 2022 Broadcom. All rights reserved. The term "Broadcom"
refers solely to the Broadcom Inc. corporate affiliate that owns
the software below.
This work is licensed under the OpenAFC Project License, a copy of which is included with this software program.

# New User Creation
New users can be created via the CLI or can be registered via the Web GUI

# CLI to create users
```
rat-manage-api user create --role Admin --role AP --role Analysis --org "MyCompany" myusername "Enter Your Password Here"
```
If `--org` is specified and the organization does not exist, it will be automatically created in the database (`aaa_org`) and assigned to the user.

## CLI to assign roles and organization
Roles and organization for existing users can be modified using CLI.
Note that for users added via "user create" command, the email and username are the same.
```
rat-manage-api user update --role Admin --role AP --role Analysis --org "MyCompany" --email "user@mycompany.com"
```

# Organizations and Multi-Tenancy

OpenAFC partitions tenant data using organizations (`aaa_org` table). Every user's tenant affiliation is stored in the `org` field of `aaa_user`.

## Role Scoping and Permissions

- **`Admin` (Tenant Administrator):**
  Administrative operations performed by an `Admin` user are strictly scoped to the user's assigned organization (`org`). An `Admin` can only manage users, assign non-Super roles, and view resources belonging to their own organization.
- **`Super` (Platform Superuser):**
  Platform-wide administrator who transcends organization boundaries. Only `Super` users can assign the `Super` role, manage accounts across different organizations, and modify site-wide settings such as minimum EIRP limits (`/user/eirp_min`).
- **Unassigned Accounts (`org = ""`):**
  Accounts without an assigned organization (such as newly registered OIDC accounts) have no tenant domain. Tenant-scoped administrative actions are restricted until an organization is assigned or the account is granted the `Super` role.

## Creating and Assigning Organizations

Organizations are created and assigned to users in three ways:

### 1. During User Creation (CLI)
Pass `--org <org_name>` to `rat-manage-api user create`:
```bash
docker exec -it <rat_server_container> rat-manage-api user create \
    --role Admin --role AP --role Analysis \
    --org "Broadcom" \
    --email "admin@broadcom.com" \
    admin_user "Password123"
```
The command automatically checks if the organization exists in `aaa_org`, inserts it if absent, and associates the user with that organization.

### 2. Updating an Existing Account (CLI)
Pass `--org <org_name>` to `rat-manage-api user update`:
```bash
docker exec -it <rat_server_container> rat-manage-api user update \
    --email "user@broadcom.com" \
    --org "Broadcom" \
    --role Admin --role AP
```
This updates the user's organization in `aaa_user` and creates the organization entry in `aaa_org` if it does not already exist.

### 3. Direct Database Provisioning (e.g. for OIDC SSO Users)
When users log in via OIDC Single Sign-On for the first time, their account row is created with an empty organization (`org = ""`). An administrator can create the organization and assign it to the user directly in PostgreSQL:
```bash
docker exec <ratdb_container> psql -U postgres -d fbrat -c "
  INSERT INTO aaa_org (name) VALUES ('Broadcom') ON CONFLICT DO NOTHING;
  UPDATE aaa_user SET org = 'Broadcom' WHERE email = 'user@broadcom.com';
"
```

To list existing organizations in the database:
```bash
docker exec <ratdb_container> psql -U postgres -d fbrat -c "SELECT id, name FROM aaa_org;"
```

# Web User Registration
New user can request an account on the web page.  For non-OIDC method, use the register button and follow the instructions sent via email.
For OIDC signin method, use the About link to fill out the request form.  If a request for access is granted, an email reply informs the user on next steps to get on board.  Custom configuration is required on the server to handle new user requests. See Customization.md for details.
Regardless of method newly granted users will have default Trial roles upon first time the person logs in.  The Admin or Super user can use the web GUI to change a user roles

# New User access
New Users who are granted access automatically have Trial roles and have access to the Virtual AP tab to submit requests.  
Test user can chose from **TEST_US** **TEST_CA** or **TEST_BR** in the drop down in the Virtual AP tab.  The corresponding config for these test regsions should first be set in AFC Config tab by the admin. New Users who are granted access automatically have Trial roles and have access to the Virtual AP tab to submit requests.


