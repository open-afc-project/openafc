FROM nginx:1.21.6
RUN apt update \
    && apt install -y openssl \
    && apt install -y bash
COPY nginx/nginx.conf /etc/nginx/
RUN mkdir -p /wd
