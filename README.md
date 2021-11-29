This work is licensed under the OpenAFC Project License, a copy of which is included with this software program.

<br />
<br />

## Table of Contents
- [**Introduction**](#introduction)
- [**Contributing**](#contributing)
  - [How to contribute](#how-to-contribute)
  - [Pull request best practices](#pull-request-best-practices)
    - [Step 1: File an issue](#step-1-file-an-issue)
    - [Step 2: Clone OpenAFC GitHub repository](#step-2-clone-openafc-github-repository)
    - [Step 3: Create a temporary branch](#step-3-create-a-temporary-branch)
    - [Step 4: Commit your changes](#step-4-commit-your-changes)
    - [Step 5: Rebase](#step-5-rebase)
    - [Step 6: Run the tests](#step-6-run-the-tests)
    - [Step 7: Push your branch to GitHub](#step-7-push-your-branch-to-github)
    - [Step 8: Send the pull request](#step-8-send-the-pull-request)
      - [Change Description](#change-description)
- [**How to Build**](#how-to-build)
- [AFC Engine build in docker setup](#afc-engine-build-in-docker-setup)
  - [Installing docker engine](#installing-docker-engine)
  - [Building the Docker image](#building-the-docker-image)
    - [Prerequisites:](#prerequisites)
    - [Building Docker image from Dockerfile (can be omitted once we have Docker registry)](#building-docker-image-from-dockerfile-can-be-omitted-once-we-have-docker-registry)
    - [Pulling the Docker image from Docker registry](#pulling-the-docker-image-from-docker-registry)
  - [Building OpenAFC engine server](#building-openafc-engine-server)

<br /><br />

# **Introduction**

This document describes the procedure for submitting the source code changes to the TIP's openAFC github project. Procedure described in this document requires access to TIP's openAFC project and knowledge of the GIT usage. Please contact support@telecominfraproject.com in case you need access to the openAFC project.
Github.com can be referred for [details of alternate procedures for creating the pull requests](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/about-pull-requests), developers can use any of these methods but need to include change description as part of pull requests description.


<br /><br />

# **Contributing**

All contributions are welcome to this project.

## How to contribute

* **File an issue** - if you found a bug, want to request an enhancement, or want to implement something (bug fix or feature).
* **Send a pull request** - if you want to contribute code. Please be sure to file an issue first.

## Pull request best practices

We want to accept your pull requests. Please follow these steps:

### Step 1: File an issue

Before writing any code, please file an issue stating the problem you want to solve or the feature you want to implement. This allows us to give you feedback before you spend any time writing code. There may be a known limitation that can't be addressed, or a bug that has already been fixed in a different way. The issue allows us to communicate and figure out if it's worth your time to write a bunch of code for the project.

### Step 2: Clone OpenAFC GitHub repository

OpenAFC source repository can be cloned using the below command.
```
git clone git@github.com:Telecominfraproject/open-afc.git
```
This will create your own copy of our repository.
[about remote repositories](https://docs.github.com/en/get-started/getting-started-with-git/about-remote-repositories)

### Step 3: Create a temporary branch

Create a temporary branch for making your changes.
Keep a separate branch for each issue/feature you want to address.
```
git checkout -b my_changes
```

### Step 4: Commit your changes
As you develop code, commit your changes into your local feature branch.
Please make sure to include the issue number you're addressing in your commit message.
This helps us out by allowing us to track which issue/feature your commit relates to.
Below command will commit your changes to the local branch.
```
git commit -a -m "Issue:123  desctiption of the change  ..."
```
### Step 5: Rebase

Before sending a pull request, rebase against upstream, such as:

```
git fetch origin
git rebase origin
```
This will add your changes on top of what's already in upstream, minimizing merge issues.

### Step 6: Run the tests

TBD
Make sure that all tests are passing before submitting a pull request.

### Step 7: Push your branch to GitHub

Push code to your remote feature branch.
Below command will push your local branch along with the changes to OpenAFC GitHub.
```
git push -u  origin my_change
```
 > NOTE: The push can include several commits (not only one), but these commits should be related to the same logical change/issue fix/new feature originally described in the [Step 1](#step-1-file-an-issue).

### Step 8: Send the pull request

Send the pull request from your feature branch to us.

#### Change Description


When submitting a pull request, please use the following template to submit the change description, risks and validations done after making the changes
(not a book, but an info required to understand the change/scenario/risks/test coverage)

- Issue Description: Issue # (from [Step 1](#step-1-file-an-issue)). A brief description of issue(s) being fixed and likelihood/frequency/severity of the issue,   or description of new feature if it is a new feature.
- Reproduction procedure: Details of how the issue could be reproduced / procedure to reproduce the issue.
Description of Change:  A detailed description of what the change is and assumptions / decisions made
- Risks: Low, Medium or High and reasoning for the same.
- Fix validation procedure:
  - Description of validations done after the fix.
  - Required regression tests: Describe what additional tests should be done to ensure no regressions in other functionalities.
  - Sanity test results as described in the [Step 6](#step-6-run-the-tests)

> NOTE: Keep in mind that we like to see one issue addressed per pull request, as this helps keep our git history clean and we can more easily track down issues.

<br /><br />
<br /><br />

# **How to Build**
# AFC Engine build in docker setup

## Installing docker engine
Docker engine instructions specific to your OS are available on [official website](https://docs.docker.com/engine/install/)

## Building the Docker image
(Building the Docker image with build environment can be omitted once we have Docker registry)

### Prerequisites:

Currently, all the prerequisites (except docker installation) are situated in [OpenAFC project's GitHub](https://github.com/Telecominfraproject/open-afc). All you need is to clone OpenAFC locally to your working directory and start all following commands from there.
In this doc we assume to work in directory /tmp/work

### Building Docker image from Dockerfile (can be omitted once we have Docker registry)

This can take some time

```
docker build . -t fbrat-build -f Dockerfile-for-build
```

### Pulling the Docker image from Docker registry

Not available currently. Possibly will be added later.

## Building OpenAFC engine server

Building and installing the fbrat with ninja-build is seamless - if you run the container without special command - it will execute the script from the CMD directive in Dockerfile.

**NB:** "-v" option maps the folder of the real machine into the insides of the docker container.

&quot;-v /tmp/work/open-afc:/wd/fbrat&quot; means that contents of &quot;/tmp/work/open-afc&quot; folder will be available inside of container in /wd/fbrat/

```
 docker run --rm -it -v \`pwd\`:/wd/fbrat fbrat-build
```
If you want to build the rpm you will need to run it with Docker:

```
docker run --rm -it -v \`pwd\`:/wd/fbrat fbrat-build /wd/fbrat/build-rpm.sh
```
