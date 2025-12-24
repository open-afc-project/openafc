#!/bin/sh
#
# Copyright 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

result=success
for region in $SWEEP_REGIONS
do
    /usr/bin/rat-manage-api cert_id sweep --country $region || result=fail
done
if [ "$result" == "success" ]
then
	echo Certificates for $SWEEP_REGIONS successfully downloaded
	touch /wd/touchfile
fi
