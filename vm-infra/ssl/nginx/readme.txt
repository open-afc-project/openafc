download the new cert for nginx 
for nginx the ssl cert is expected to be in .pem format ( not crt)
the pem format should include :
1. individual Certificate afc.broadcom.com 
2. Intermediate certificate DigiCert TLS RSA SHA256 2020 CA1

So, download cert, by default is is named as: afc_broadcom_com.pem
and rename it to server.cert.pem and restart nginx or whole server 
1. mv afc_broadcom_com.pem server.cert.pem
2. docker-compose down -v && docker-compose up -d; docker-compose logs -f


Note:

If you have only the crt file, append CA to the 
end of it and rename to server.cert.pem  

1.mv afc_broadcom_com.crt server.cert.pem
2.cat DigiCertCA.crt >>server.cert.pem


