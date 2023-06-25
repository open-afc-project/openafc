#!/bin/sh
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
make_key()
{
    local k_name=$1
    local cmd="openssl genrsa -out $k_name/$k_name"_key.pem" 4096 > /dev/null 2>&1"
    eval $cmd
    if [ "$?" -ne "0" ]; then echo -e "ERROR: Failed to generate key $k_name\n" ; return 1 ; fi
}
make_client_cert()
{
    local cli_name=${1:-test}"_cli"
    local ca_path=${AFC_CA_CERT_PATH:-.}
    local ca_crt="test_ca_crt.pem"
    local ca_key="test_ca_key.pem"
    local cli_addr=$(hostname -i)

    mkdir $cli_name
    make_key $cli_name
    openssl req -new -key $cli_name/$cli_name"_key.pem" \
        -out $cli_name/$cli_name".csr" -sha256 \
        -subj "/C=IL/ST=Israel/L=Tel Aviv/O=Broadcom/CN=$cli_name" > /dev/null 2>&1

    cat << EOF > $cli_name"_ext.cnf"
authorityKeyIdentifier=keyid,issuer
basicConstraints = CA:FALSE
extendedKeyUsage=clientAuth
keyUsage = critical, digitalSignature, keyEncipherment
subjectAltName = IP:$cli_addr
subjectKeyIdentifier=hash
EOF
    cat << EOF > $cli_name".cnf"
HOME		= .

[ ca ]
default_ca      = CA_default

[ CA_default ]
dir		= $cli_name
certs		= $cli_name
database	= $cli_name/index.txt
new_certs_dir	= $cli_name
serial		= $cli_name/serial.txt
policy		= policy_default
default_md	= sha256

[ policy_default ]
EOF
    touch $cli_name/index.txt
    echo 01 > $cli_name/serial.txt
    openssl ca -config $cli_name".cnf" \
        -startdate $(date --date='-7 days' +'%y%m%d000000Z') \
        -days 29 -batch -notext \
        -out $cli_name/$cli_name"_crt.pem" -cert $ca_path/$ca_crt \
        -keyfile $ca_path/$ca_key \
        -in $cli_name/$cli_name".csr" -extfile $cli_name"_ext.cnf" > /dev/null 2>&1
    # echo "\nCreated certificate."
    openssl x509 -startdate -enddate -noout -in $cli_name/$cli_name"_crt.pem" > /dev/null
    return 0
}
#
#
#
case $1 in
    cert_client)
        shift
        make_client_cert $1
    ;;
    ''|*)
        echo -e "Nothing todo ($1)\n"
    ;;
esac

exit 
