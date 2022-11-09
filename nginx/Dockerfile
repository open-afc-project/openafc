FROM nginx:1.21.6
RUN apt update \
    && apt install -y openssl \
    && apt install -y bash
RUN mkdir -p /certificates /certificates/servers /certificates/clients
# server side certificates
COPY certs/servers/server.cert.pem /certificates/servers/
COPY certs/servers/server.key.pem /certificates/servers/
COPY certs/clients/client.bundle.pem /certificates/clients/
# client side certificates
COPY certs/servers/server.bundle.pem /certificates/servers/
COPY certs/clients/client.key.pem /certificates/clients/
COPY certs/clients/client.bdl.pem /certificates/clients/
COPY nginx.conf /etc/nginx/
RUN mkdir -p /wd
