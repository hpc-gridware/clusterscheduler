# Latest (unstable) Open Cluster Scheduler images (Ubunutu 22.04)

This repository contains Dockerfiles and scripts to build the latest (unstable) Open Cluster Scheduler packages in an Ubuntu 22.04 base image and publish them to Docker Hub at [https://hub.docker.com/repository/docker/hpcgridware/clusterscheduler-latest-ubuntu2204/general](https://hub.docker.com/repository/docker/hpcgridware/clusterscheduler-latest-ubuntu2204/general).

# Resulting Container images

## hpcgridware/clusterscheduler-latest-ubuntu2204

Ubuntu 22.04 base images with the latest Open Cluster Scheduler packages installed in /opt/ocs. This images does not contain any 3rd party/OS packages required to build Open Cluster Scheduler from the sources.

## hpcgridware/clusterscheduler-builder-ubuntu2204

Ubuntu 22.04 based image with all required 3rd party/OS packages to build Open Cluster Scheduler from the sources.
