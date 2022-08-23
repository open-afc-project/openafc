#!/bin/bash
cd /var/lib/fbrat/daily_uls_parse
python daily_uls_parse.py
cp /var/lib/fbrat/ULS_Database/*.sqlite3 /output_folder/