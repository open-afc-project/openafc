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
  - [Export tests from WFA file to JSON format](#export-tests-from-wfa-file-to-json-format)
  - [Export tests requests and responses in WFA file format](#export-tests-requests-and-responses-in-wfa-file-format)
  - [Compare AFC config records](#compare-afc-config-records)
  - [Reacquisition response records for exist requests](#reacquisition-response-records-for-exist-requests)
- [**Testing setup**](#testing-setup)
- [**Change testing database manually**](#change-testing-database-manually*)

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
afc_tests.py --addr <server address> --cmd run [--testcase_indexes <test index>] [--testcase_ids <test ids>]
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
afc_tests.py --addr 1.2.3.4 --cmd run
```
Run a test or number of tests according to test case row index
```
afc_tests.py --addr 1.2.3.4 --cmd run --testcase_indexes 3,5,6,7
```
Run a test or number of tests according to test case ids
```
afc_tests.py --addr 1.2.3.4 --cmd run --testcase_ids AFCS.IBP.5, AFCS.FSP.18
```
Run a test and save test result to csv format file.
```
afc_tests.py --addr 1.2.3.4 --outfile=filename.csv --cmd run --testcase_indexes 22
```
Start run all tests and save results as ‘golden reference’ in the database
```
afc_tests.py --addr 1.2.3.4 --cmd add_reqs
```
Run the 1st test from test DB explicitly use HTTP
```
afc_tests.py --addr 1.2.3.4 --prot http --port 80 --testcase_indexes 1 --cmd run
```

## Add new test vectors

Add new test vectors to the database provide a file to the following command line.
```
afc_tests.py --addr=1.2.3.4 --cmd add_reqs --infile add_test_vector.txt
```
Provided file consists of any number of test vectors in the following format of AFC request.

{"availableSpectrumInquiryRequests": [{"inquiredChannels": [{"globalOperatingClass": 131}, {"globalOperatingClass": 132}, {"globalOperatingClass": 133}, {"globalOperatingClass": 134}], "deviceDescriptor": {"rulesetIds": ["US_47_CFR_PART_15_SUBPART_E"], "serialNumber": "Alpha001", "certificationId": [{"nra": "AlphaNRA01", "id": "AlphaID01"}]}, "vendorExtensions": [], "inquiredFrequencyRange": [{"lowFrequency": 5925, "highFrequency": 6425}, {"lowFrequency": 6525, "highFrequency": 6875}], "minDesiredPower": 18, "location": {"indoorDeployment": 2, "elevation": {"verticalUncertainty": 5, "heightType": "AGL", "height": 129}, "ellipse": {"orientation": 45, "minorAxis": 50, "center": {"latitude": 40.75924, "longitude": -73.97434}, "majorAxis": 100}}, "requestId": "1"}], "version": "1.1"}, {"testCase": "AFC.FSP.1”, "optonal":”optional”}

By default, a new test vector is sent to the AFC server in order to acquire it’s response and keep it in the database.

## Dry-run test vectors

This is an option to run a test vector from a file without further saving it or it's response to the database file.
```
afc_tests.py --addr 1.2.3.4 --cmd  dry_run --infile add_test_vector.txt
```

## Dump test database

Release testing database binary together with SQLITE dump file.
Steps to produce such dump file (dump.sql).
```
sqlite3 afc_input.sqlite3
>
> .output dump.sql
> .dump
> .quit
```

Show all entries from the database (requests and responses)
```
afc_tests.py --cmd dump_db
```

## Export AFC Server admin configuration

Export from the testing database to text file in JSON format.
```
afc_tests.py --cmd exp_adm_cfg  --outfile export_admin_cfg.json
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

## Export tests from WFA file to JSON format

How to export test vectors from WFA file into JSON format file.
```
afc_tests.py --cmd parse_tests --infile <input file> --outfile <output file>
```
For example, export certain WFA test cases

```
afc_tests.py --cmd parse_tests --infile "AFC System (SUT) Test Vectors r6.xlsx" --outfile abc.txt --test_id srs
```

## Export tests requests and responses in WFA file format

Following example exports test vectors and corresponded responses to WFA format files.
All files created in local directory "wfa_test".
```
afc_tests.py --cmd dump_db --table wfa --addr <ip> --outfile <output file>
```
An example how to use it with docker container, named "test".
Following sequence creates files at path "`pwd`/data/wfa_test".

```
docker -rm -v `pwd`/data:/data test --cmd dump_db --table wfa --addr <ip> --outpath /data
```

## Compare AFC config records

Compare provided AFC configuration with all/any records in the database.
```
afc_tests.py --cmd cmp_cfg --infile ../../afc_config.json
```
```
afc_tests.py --cmd cmp_cfg --infile ../../afc_config.json --idx 1
```

## Reacquisition response records for exist requests

Re-run tests from the database and update corresponded responses.
```
afc_tests.py --cmd reacq --addr 1.2.3.4
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

# **Change testing database manually**

The procedure requires to drop the DB into a SQL file, edit if needed and create a new DB based on changed SQL file.

Open the DB and dump to a SQL file.
```
sqlite3 afc_input.sqlite3
>
> .output dump.sql
> .dump
> .quit
```
Open and edit dump.sql with any editor as it is a text file with not complicated SQL code.

Make a backup from original DB.
```
mv ./afc_input.sqlite3 ./afc_input.sqlite3_backup
```
Open a new file and create tables in it.
```
sqlite3 ./afc_input.sqlite3
>
> .read dump.sql
> .quit
```
At this stage there is a fixed DB that can be used.

<br /><br />

Happy usage!
