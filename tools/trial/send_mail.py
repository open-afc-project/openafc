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
from email.message import EmailMessage
import json
import argparse
##### 
### configurations
### config file is in json of this format:
#{"sender_email":"sender email here","password":"password here","port":"465"}
####

parser = argparse.ArgumentParser(description='Read and parse request json')
parser.add_argument('--req', type=str, required=True,
                    help='request json file name')
parser.add_argument('--resp', type=str, required=True,
                    help='response json file name')
parser.add_argument('--recvr', type=str, required=True,
                    help='receiver email address')
parser.add_argument('--cc_email', type=str, required=False,
                    help='cc email (group) address')
parser.add_argument('--config', type=str, required=True,
                    help='config file in json format')
args = parser.parse_args()

receiver_email = args.recvr
req_file = args.req
resp_file = args.resp
conf_file = args.config
cc_email = args.cc_email

f = open(req_file)
req_data = json.load(f)['availableSpectrumInquiryRequests'][0]
req_id = req_data['requestId']

f.close()
f = open(conf_file)
config_data = json.load(f)
password = config_data['password']
sender_email = config_data['sender_email']
port = int(config_data['port'])

# Create a secure SSL context
context = ssl.create_default_context()

with smtplib.SMTP_SSL("smtp.gmail.com", port, context=context) as server:
    server.login(sender_email, password)
    body = f"Please find attached response for {req_id}"

    message = EmailMessage()
    message['Subject'] = f"[Broadcom AFC Trials] Response for request id: {req_id}"
    message['From'] = sender_email
    message['To'] = receiver_email
    if cc_email:
        message['Cc'] = cc_email
    else:
       cc_email = ""

    message.add_alternative(body)
    with open(req_file, "rb") as attachment:
        message.add_attachment(attachment.read(), maintype='application',
               subtype='octet-stream', filename=req_file)
    with open(resp_file, "rb") as attachment:
        message.add_attachment(attachment.read(), maintype='application',
               subtype='octet-stream', filename=resp_file)

    server.sendmail(sender_email, [receiver_email, cc_email], message.as_string())
