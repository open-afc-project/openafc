#
# **Building in Docker**

#
**AFC Engine build in docker setup**

## Building the Docker image with build environment (can be omitted once we have Docker registry)

### Installing docker engine

Docker engine instructions specific to your OS are available on [official website](https://docs.docker.com/engine/install/)

### Prerequisites:

Currently, all the prerequisites (except docker installation) are situated in [OpenAFC project's GitHub](https://github.com/Telecominfraproject/open-afc). All you need is to clone OpenAFC locally to your working directory and start all following commands from there.
In this doc we assume to work in directory /tmp/work

### Building Docker image from Dockerfile (can be omitted once we have Docker registry)

This can take some time

| $ docker build . -t fbrat-be -f Dockerfile-for-build |
| --- |

## Pulling the Docker image from Docker registry

Not available currently. Possibly will be added later.

## Building OpenAFC engine server

Building and installing the fbrat with ninja-build is seamless - if you run the container without special command - it will execute the script from the CMD directive in Dockerfile.

**NB:** "-v" option maps the folder of the real machine into the insides of the docker container.

&quot;-v /tmp/work/open-afc:/wd/fbrat&quot; means that contents of &quot;/tmp/work/open-afc&quot; folder will be available inside of container in /wd/fbrat/

| $ docker run --rm -it -v \`pwd\`:/wd/fbrat fbrat-build |
| --- |

If you want to build the rpm you will need to run it with Docker:

| $ docker run --rm -it -v \`pwd\`:/wd/fbrat fbrat-build /wd/fbrat/build-rpm.sh |
| --- |


