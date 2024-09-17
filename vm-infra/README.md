# AFC Docker Compose Project

This project sets up an Automated Frequency Coordination (AFC) system using Docker Compose. It includes various services for handling AFC requests, database management, object storage, and more.

## Project Structure

The project is organized as follows:

```txt
/opt/afc/wd/
├── bulk_pgdata/          # Mount point for bulk_postgres data - will be created on the first run. SHould be empty on first start
├── docker-compose.yaml   # Docker Compose configuration
├── OIDC/                 # OpenID Connect configuration
├── pgdata/               # Mount point for ratdb data - will be created on the first run. SHould be empty on first start
├── rat_server-conf/      # RAT server configuration for apache
├── secrets/              # Secret files
├── ssl/                  # SSL/TLS certificates and keys
├── storage/              # Mount point for object storage
├── .env                  # Configuration file with environment variables
└── README.md             # This readme file
```

## Configuration

The project is configured using a combination of environment variables (defined in `.env`) and Docker Compose settings.

### Key Configuration Files

- `.env`: Contains environment variables for configuring various aspects of the system.
- `docker-compose.yaml`: Defines the services, networks, and volumes for the Docker Compose setup.

### Important Environment Variables

- `AFC_SERVER_NAME`: Hostname for the AFC server- `AFC_ENFORCE_HTTPS`: Whether to forward all HTTP requests to HTTPS
- `VOL_H_DB`: Host static DB root directory
- `VOL_C_DB`: Container's static DB root directory
- `AFC_REQ_SERVER`: Service handling AFC Requests (msghnd or afcserver)
- `EXT_PORT`: External HTTP port range
- `EXT_PORT_S`: External HTTPS port range

Refer to the `.env` file for more detailed configuration options.

## Services

The project includes the following key services:

- `ratdb`: Main database service
- `rmq`: RabbitMQ message broker
- `dispatcher`: Nginx-based dispatcher service
- `rat_server`: Main AFC server
- `msghnd`: Message handler service
- `objst`: Object storage service
- `worker`: Worker service for processing tasks
- `als_kafka`: Apache Kafka service for logging
- `als_siphon`: Siphon service for log processing
- `bulk_postgres`: PostgreSQL database for bulk data
- `uls_downloader`: ULS (FCC Universal Licensing System) data downloader
- `cert_db`: Certificate database service
- `rcache`: Response cache service

## Getting Started

1. Ensure Docker and Docker Compose are installed on your system.
2. Clone this repository to your local machine.
3. Navigate to the project directory: `cd /opt/afc/wd`
4. Copy the  `.env` file and modify it as needed: `cp .env.example .env`
5. Set up your SSL certificates and key in the `ssl/nginx` directory if using HTTPS.
6. Remove all files from folders `pgdata/` and `bulk_pgdata`
7. Start the services: `docker-compose up -d`

## Security Notes

- The `secrets/` directory contains sensitive information. Ensure it is properly secured.
- SSL/TLS certificates in the `ssl/` directory should be kept secure and up-to-date.

## Troubleshooting

- Check service logs: `docker-compose logs [service_name]`
- Ensure all required ports are open and not conflicting with existing services.
- Ensure on clear deployment that folders `pgdata/` and `bulk_pgdata` are empty.
- Ensure ssl certs and keys are configured and named properly.
- Verify that all environment variables in `.env` are correctly set.
