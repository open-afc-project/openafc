# Prepare and Run test:
1. Install Python3 and python module dependencies
2. Copy certificate and key to cert/ (dummy files are in git). These are needed for mtls.
3. Create your token and credentials json files.  Instructions are found here: _https://developers.google.com/sheets/api/quickstart/python_
4. Populate proper values in config.json [see below]
5. ./runAllTests.sh <csvfile> <results-dir> <dbfile> <configfile> <email>
   - csvfile: downloaded google sheet in csv format
   - results-dir: where the result logs are kept.
   - dbfile is the sqlite3 db to track already run requests.
   - email: "To" email
   - configfile: json file which has among configurations, 
      the gmail account information + key/certificates. [see below]

   The requests are recorded in the db, and will be skipped next time the command is run. 
   You need to clear the sqlite3 db if you want to run everything all over again: rm <dbfile>

6. Optional: The user can choose to pick which request and response file to send: 
python3 send_mail.py --req <request-json-file> --resp <resp-json-file> --recvr <receiver-email> --conf config.json
Optional: --cc_email <email> can be used for cc

# Config file format:
```
{
"sender_email":"sender@your-inc","password":"your-email-app-password","port":"465",
"cert":"path-to-cert", "key":"path-to-key","cc_email":"your-cc@your-inc", "afc_query":"query uri","start_row":"somerow","dry_run":"true/false"
}
```
query_uri is the uri for the query, eg. _https://afc.broadcom.com/fbrat/ap-afc/availableSpectrumInquiry_
Password for your email can be set up differently depending on which email service.  For gmail, you will need to log into your google account to set it up
Optional fields: cc_email, start_row
    cc_email: cc email
    start_row: start requesting from this row in the spread sheet
    dry_run: "true" or "false".  If true, the completed requests won't be put in the database, and will be repeated in next run


# Prune test database
requests that are completed won't be repeated in next run.  This is done by insert the UID in a database.  To re-run all queries, delete the database.  To prune selectively some UID in the database, use prune_db.py e.g.
python3 prune_db.py  --csv prune.csv --db test.db
The test.db is the database
prune csv is the file listing the UID to remove
Format is as follows:
\# UID
first_uid
second_uid
...
