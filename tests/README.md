This work is licensed under the OpenAFC Project License, a copy of which is included with this software program.

<br />
<br />

## Table of Contents
- [**Introduction**](#introduction)
- [**Description**](#introduction)
- [**Basic functionality**](#basic-functionality)
  - [Testing sequence](#testing-sequence)
  - [Set AFC Server configuration](#set-afc-server-configuration)
  - [Test database](#test-database)
- [**Commands and procedures**](#commands-and-procedures)
  - [Tests execution options](#tests-execution-options)
  - [Add new test vectors](#add-new-test-vectors)
  - [Dry-run test vectors](#dry-run-test-vectors)
  - [Dump test database](#dump-test-database)
  - [Export AFC Server admin configuration](#export-afc-server-admin-configuration)
  - [Add AFC Server admin configuration](#add-afc-server-admin-configuration)
  - [Remove AFC Server admin configuration](#remove-afc-server-admin-configuration)
  - [List AFC Server admin configuration](#list-afc-server-admin-configuration)
- [**Testing setup**](#testing-setup)

- [Back to main readme](/README.md)
<br /><br />

# **Introduction**

This document describes the setup for testing environment to the TIP's openAFC github project.
In addition, describes the execution procedure for the tests.
Please contact support@telecominfraproject.com in case you need access to the openAFC project.

<br /><br />


# **Description**

The system consists of a testing utility that communicates with the AFC server.
The utility sends WFA standard AFC requests and gets AFC responses. It also provides
an analysis of the results. The utility also can be used to keep and export the AFC Server
administration configuration used during validation. It consists of users, APs,
and server configurations.

<br /><br />


# **Basic functionality**
## Testing sequence

The sequence executes all required tests and returns fail if even one test fails.
The testing procedure begins by preparing a test database with test vectors and responses.
Follows run of test utility with server and test options (for details see #tests-execution-options).
```
afc_tests.py --cfg <server cfg> --test <test cfg>
```
Current implementation provides an already prepared database file (afc_input.sqlite3).

## Set AFC Server configuration

There is a need to configure AFC Server prior testing. The configuration parameters include a list of APs,
their users and AFC Server configurations per predefined user.
Those parameters are stored in the test database and may be exported to an external JSON file to be used on the AFC server.
The procedure of configuring the AFC server has 2 steps. To export parameters from the testing database into JSON file
(#export-afc-server-admin-configuration) and to provide it to the local configuration utility (#add-afc-server-admin-configuration).

## Test database

The testing database keeps all required AFC Server configuration data (admin configuration data)
in SQL tables: user_config, ap_config, afc_config. All tables keep information as string without
splitting JSON into fields which can be redesigned by request.

<br /><br />


# **Commands and procedures**
## Tests execution options

Start sequential run of tests according to the data in the database
```
afc_tests.py --cfg addr=1.2.3.4 --test r
```
Start certain test according to the data in the database
```
afc_tests.py --cfg addr=1.2.3.4 --test r=1
```
Start run all tests and save results as ‘golden reference’ in the database
```
afc_tests.py --cfg addr=1.2.3.4 --test a
```

## Add new test vectors

To add new test vectors to the database provide a file to the following command line.
```
afc_tests.py --cfg addr=1.2.3.4 --db a=add_test_vector.txt
```
Provided file consists of any number of test vectors in the following format of AFC request.

{"availableSpectrumInquiryRequests": [{"inquiredChannels": [{"globalOperatingClass": 131}, {"globalOperatingClass": 132}, {"globalOperatingClass": 133}, {"globalOperatingClass": 134}], "deviceDescriptor": {"rulesetIds": ["US_47_CFR_PART_15_SUBPART_E"], "serialNumber": "Alpha001", "certificationId": [{"nra": "AlphaNRA01", "id": "AlphaID01"}]}, "vendorExtensions": [], "inquiredFrequencyRange": [{"lowFrequency": 5925, "highFrequency": 6425}, {"lowFrequency": 6525, "highFrequency": 6875}], "minDesiredPower": 18, "location": {"indoorDeployment": 2, "elevation": {"verticalUncertainty": 5, "heightType": "AGL", "height": 129}, "ellipse": {"orientation": 45, "minorAxis": 50, "center": {"latitude": 40.75924, "longitude": -73.97434}, "majorAxis": 100}}, "requestId": "1"}], "version": "1.1"}

By default, a new test vector is sent to the AFC server in order to acquire it’s response and keep it in the database.

## Dry-run test vectors

This is an option to run a test vector from a file without further saving it or it's response to the database file.
```
afc_tests.py --cfg addr=1.2.3.4 --db r=add_test_vector.txt
```
## Dump test database

Show all entries from the database (requests and responses)
```
afc_tests.py --db d
```

## Export AFC Server admin configuration

Export from the testing database to text file in JSOn format.
```
afc_tests.py --db e=./export_admin_cfg.json
```

## Add AFC Server admin configuration

Read configurations from provided file and set AFC server
```
rat-manage-api cfg add src=./add_admin_cfg.json
```

## Remove AFC Server admin configuration

Read configurations from provided file and remove them from AFC server
```
rat-manage-api cfg del src=./del_admin_cfg.json
```

## List AFC Server admin configuration

List internal data from provided admin configuration file
```
rat-manage-api cfg list src=./add_admin_cfg.json
```

<br /><br />


# **Testing setup**

The testing setup consists of utility and database.
The utility provides options to send AFC request, to receive AFC response,
analyze the response, acquire response data, export AFC server admin data
to server admin configuration file. The admin server configuration file has
data required for creation of server test users and server test APs.
Each server user is provided with corresponding AFC server configuration.
The AFC server admin configuration file is in JSON format by following structure:
```
{“afcAdminConfig”:
           {“afcConfig: {...}},
            “userConfig: { “username”: ”alpha”, “password”: ”123456”, “rolename”: [“AP”, “Admin”]}, 
            “apConfig”: [ 
                                 { “serialNumber”: ”Alpha01”, “certificationId”: [{ “”nra”:”AlphaNRA02”, ”id”:”AlphaID01”}]},
                                 { “serialNumber”: ”Alpha02”, “certificationId”: [{ “”nra”:”AlphaNRA02”, ”id”:”AlphaID02”}]}
                                ]
}
```
The AFC server admin configuration file provided to the AFC server utility (rat-manage-api) in order to configure the server.

There is also the ability to delete AFC server administration configuration by providing a JSON file of the following format.
```
{“afcAdminConfig”:
            “userConfig: { “username”: ”alpha”}, 
            “apConfig”: [ 
                                 { “serialNumber”: ”Alpha01”},
                                 { “serialNumber”: ”Alpha02”}
                                ]
}
```

<br /><br />

Happy usage!
