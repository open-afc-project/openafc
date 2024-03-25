## Overview
The rat_server component is an apache web server (httpd) that gives an entry point for the REST API calls and serves up the web user interface. It does not handle the MTLS or SSL authentication by default; those are managed by the dispatcher component.

The pages that are served are kept in the src/ratapi and src/web directories.

## Environment and configuration
The configuration for the server is managed by the *.conf files in this directory.  While rat_server has no specific configuration via the environment, it uses the Rabbit MQ (rmq), Object Storage (objst), ALS, and Rcache components and needs to have parameters set in accord with those components.  The example below provides the variables to be defined.  You can see the (readme)[../README.md] for the values set by the sample .env file included there. 
```
    # RabbitMQ server name:
      - BROKER_TYPE=external
      - BROKER_FQDN=rmq
      # Filestorage params:
      - AFC_OBJST_HOST=objst
      - AFC_OBJST_PORT=5000
      - AFC_OBJST_SCHEME=HTTP
      # ALS params
      - ALS_KAFKA_SERVER_ID=rat_server
      - ALS_KAFKA_CLIENT_BOOTSTRAP_SERVERS=${ALS_KAFKA_SERVER_}:${ALS_KAFKA_CLIENT_PORT_}
      - ALS_KAFKA_MAX_REQUEST_SIZE=${ALS_KAFKA_MAX_REQUEST_SIZE_}
      # Rcache parameters
      - RCACHE_ENABLED=${RCACHE_ENABLED}
      - RCACHE_POSTGRES_DSN=postgresql://postgres:postgres@bulk_postgres/rcache
      - RCACHE_SERVICE_URL=http://rcache:${RCACHE_CLIENT_PORT}
      - RCACHE_RMQ_DSN=amqp://rcache:rcache@rmq:5672/rcache
```