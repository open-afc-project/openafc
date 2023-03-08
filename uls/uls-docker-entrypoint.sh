#!/bin/sh
cd /mnt/nfs/rat_transfer/daily_uls_parse
python3 daily_uls_parse.py
cp /mnt/nfs/rat_transfer/ULS_Database/*.sqlite3 /output_folder/