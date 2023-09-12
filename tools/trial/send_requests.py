#!/usr/bin/python
# Portions copyright © 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate
# affiliate that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy
# of which is included with this software program.

import smtplib, ssl
from email import encoders
from email.mime.base import MIMEBase
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText
import sqlite3
import csv
import argparse
import requests
import json
import os
from email.message import EmailMessage

def send_mail(file, outfile, email, config_data, server, req_id, requester):
    sender_email = config_data['sender_email']
    receiver_email = email
    try:
        cc_email = config_data['cc_email']
    except:
        cc_email = ""

    body = f"Hello there,\nPlease find attached response for AFC trial requested with\n"
    body += f"Unique Request ID: {req_id}\nRequester: {requester}\n"
    body += f"\nThe attached response is in JSON format, in accordance with the " \
            "WFA AFC System to AFC Device Interface specification.\n" \
            "If you wish to manually inspect the JSON response, you might consider using a " \
            "free online parser tool such as: https://jsonformatter.org/\n\n" \
            "Best Regards,\nBroadcom AFC Trials."

    message = EmailMessage()
    message['Subject'] = f"[Broadcom AFC Trials] Response for request id: {req_id}"
    message['From'] = sender_email
    message['To'] = receiver_email
    message['Cc'] = cc_email

    message.add_alternative(body)
    with open(file, "rb") as attachment:
        message.add_attachment(attachment.read(), maintype='application',
               subtype='octet-stream', filename=file)
    with open(outfile, "rb") as attachment:
        message.add_attachment(attachment.read(), maintype='application',
               subtype='octet-stream', filename=outfile)

    server.sendmail(sender_email, [receiver_email, cc_email], message.as_string())


def run_test(uid, requester, res, email, config_data):
    headers = {
        'Content-Type': 'application/json',
    }
    params = {
        'debug': 'False',
    }
    conf_cert = config_data['cert']
    conf_key = config_data['key']
    cert = (conf_cert, conf_key)
    filename =  f'{indir}/UID_{uid}.json'
    outfilename = f'{res}/UID_{uid}.json'
    print ("Sending request UID = %s" %uid)

    # Send to requester if no override provided
    if not email:
       email = requester

    if email:
        # Create a secure SSL context
        context = ssl.create_default_context()
        port = int(config_data['port'])
        server =  smtplib.SMTP_SSL("smtp.gmail.com", port, context=context)
        password = config_data['password']
        server.login(config_data['sender_email'], password)
    else:
        server = None

    with open(filename) as f:
        data = f.read().replace('\n', '').replace('\r', '').encode()
        f.close()
        response = requests.post(
            config_data['afc_query'],
            params=params,
            headers=headers,
            data=data,
            cert=cert,
            verify='/etc/ssl/certs/ca-bundle.crt',
        )

        print("Filter vendor extensions")
        # Delete the vendor extensions from public trial results
        filtered_response = response.json()

        try:
            del filtered_response["availableSpectrumInquiryResponses"][0]["vendorExtensions"]
        except KeyError:
            print ("No vendor entension!")
        except IndexError:
            print ("Unknown response format!")

        fout = open(outfilename, "w")
        fout.write(json.dumps(filtered_response, indent=1))

        fout.close()
        send_mail(filename, outfilename, email, config_data, server, uid, requester)

def init_db(conn):
    # create cursor object
    cur = conn.cursor()

    # check if table exists
    listOfTables = cur.execute(
      """SELECT name FROM sqlite_master WHERE type='table'
      AND name='REQUESTS'; """).fetchall()
    
    if listOfTables == []:
        print('Table not found!')
        conn.execute('''CREATE TABLE REQUESTS
             (ID INTEGER PRIMARY KEY AUTOINCREMENT  ,
             UID           TEXT    NOT NULL,
             TIMESTAMP     TEXT     NOT NULL);''')
        print("Table created successfully")

parser = argparse.ArgumentParser(description='Read and parse request json')
parser.add_argument('--csv', type=str, required=True,
                    help='request csv file name')
parser.add_argument('--res', type=str, required=True,
                    help='result directory')
parser.add_argument('--db', type=str, required=True,
                    help='result directory')
parser.add_argument('--config', type=str, required=True,
                    help='result directory')
parser.add_argument('--email', type=str, required=False,
                    help='To email address')
args = parser.parse_args()

if not os.path.exists(args.res):
    print ("Make result directory %s " %args.res)
    os.mkdir(args.res)

req_file = args.csv
indir = os.path.dirname(req_file)

if not dir:
   indir = os.getcwd()

conn = sqlite3.connect(args.db)
print ("Opened database %s successfully" %args.db)

conf_file = args.config
f = open(conf_file)
config_data = json.load(f)
if args.email:
    email = args.email
else:
    email = None

f.close()

# initialize db if not already exists.
init_db(conn)

# run tests.
with open(req_file, 'r') as csv_file:
    reader = csv.DictReader(csv_file)
    i = 2
    try:
        start_row = int(config_data['start_row'])
    except:
        start_row = 2

    for row in reader:
        ts = row['Timestamp']
        uid = row['Unique request ID'] + f'_R{i}'
        requester = row['Email address']
        i = i+1
        list = conn.execute("SELECT * from REQUESTS WHERE UID=?", (uid,)).fetchall()
        if list == [] and i > start_row: 
            print("Querying for uid %s" %uid)
            conn.execute("INSERT INTO REQUESTS (TIMESTAMP,UID) \
                VALUES (?, ?)", (ts, uid));
            conn.commit()
            run_test(uid, requester, args.res, email, config_data)

        else:
            print("Ignore existing uid %s" %uid)


conn.close()
