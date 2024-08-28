#___INFO__MARK_BEGIN_NEW__
###########################################################################
#
#  Copyright 2024 HPC-Gridware GmbH
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
###########################################################################
#___INFO__MARK_END_NEW__

# Use Ubuntu 22.04 LTS as the base image with build dependencies
FROM hpcgridware/clusterscheduler-builder-ubuntu2204 AS builder

ARG BUILD_COMMIT=unknown
ARG BUILD_TIME=unknown

# Set noninteractive environment to avoid prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Check out the source code from the repository
RUN git clone https://github.com/hpc-gridware/clusterscheduler /clusterscheduler/clusterscheduler

# Build and install Open Cluster Scheduler in /opt/cs
WORKDIR /build

# Add java home for cmake
ENV JAVA_HOME=/usr/lib/jvm/java-11-openjdk-amd64
RUN cmake -S /clusterscheduler/clusterscheduler
RUN make 3rdparty
RUN make install

FROM ubuntu:22.04

ARG BUILD_COMMIT=unknown
ARG BUILD_TIME=unknown

RUN apt-get update && apt-get install -y \
    binutils \
    default-jdk \
    maven \
    && rm -rf /var/lib/apt/lists/*

# Copy Open Cluster Scheduler from the builder image.
COPY --from=builder /opt/cs /opt/cs

ENTRYPOINT ["/bin/bash"]
