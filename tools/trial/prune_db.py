#!/usr/bin/python
# Portions copyright © 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate
# affiliate that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy
# of which is included with this software program.
import csv
import argparse
import sqlite3

parser = argparse.ArgumentParser(description='Read and parse request json')
parser.add_argument('--csv', type=str, required=True,
                        help='request csv file name')
parser.add_argument('--db', type=str, required=True,
                        help='result directory')

args = parser.parse_args()

csv_file = args.csv

conn = sqlite3.connect(args.db)
print ("Opened database %s successfully" %args.db)

with open(csv_file, 'r') as csv_file:
    reader = csv.DictReader(csv_file)
    for row in reader:
        uid = row['UID']
        list = conn.execute("SELECT * from REQUESTS WHERE UID=?", (uid,)).fetchall()
        if not list == []:
            print("Found and prunning %s" %uid)
            conn.execute("DELETE from REQUESTS WHERE UID=?", (uid,))
    conn.commit()
