#!/bin/awk -f

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

BEGIN {
	print "/*"
	print " * Copyright (C) 2022 Broadcom. All rights reserved."
	print " * The term \"Broadcom\" refers solely to the Broadcom Inc. corporate affiliate"
	print " * that owns the software below."
	print " * This work is licensed under the OpenAFC Project License, a copy of which is"
	print " * included with this software program."
	print " *"
	print " * This file creates ALS (AFC Request/Response/Config Logging System) database on PostgreSQL+PostGIS server"
	print " * This file is generated, direct editing is not recommended."
	print " * Intended maintenance sequence is as follows:"
	print " *   1. Load (copypaste) als_db_schema/ALS.dbml into dbdiagram.io"
	print " *   2. Modify as needed"
	print " *   3. Save (copypaste) modified sources back to als_db_schema/ALS.dbml"
	print " *   4. Also export schema in PostgreSQL format as als_db_schema/ALS_raw.sql"
	print " *   5. Rectify exported schema with als_rectifier.awk (awk -f als_db_schema/als_rectifier.awk < als_db_schema/ALS_raw.sql > ALS.sql)"
	print " */"
	print ""
	print "CREATE EXTENSION postgis;"
	print ""
    # Make all operations SQL-statement-level (not line-level)
	RS=ORS=";"
}

# Removal of unwanted tables, created by dbdiagram.io for many-to-many relations
/\w+ TABLE "device_descriptor_certification"/ {next}
/\w+ TABLE "device_descriptor_regulatory_rule"/ {next}

{ print }
