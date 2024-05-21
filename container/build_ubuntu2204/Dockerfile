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

# Use Ubuntu 22.04 LTS as the base image
FROM ubuntu:22.04

# Set noninteractive environment to avoid prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Update and install necessary packages
RUN apt-get update && apt-get install -y \
    git \
    autoconf \
    automake \
    build-essential \
    manpages-dev \
    libudev-dev \
    expect \
    tcl \
    patchelf \
    gnuplot \
    xterm \
    doxygen \
    graphviz \
    pandoc \
    cmake \
    openssl \
    libssl-dev \
    libdb5.3-dev \
    libjemalloc-dev \
    hwloc \
    libhwloc-dev \
    rapidjson-dev \
    vim \
    && rm -rf /var/lib/apt/lists/*

# Install a specific version of cmake if it is necessary. Here, install cmake 3.27.9 manually if needed.
# May need to download and compile from source if version is crucial
ADD https://github.com/Kitware/CMake/releases/download/v3.27.9/cmake-3.27.9.tar.gz /tmp/
RUN cd /tmp && \
    tar -xzvf cmake-3.27.9.tar.gz && \
    cd cmake-3.27.9 && \
    ./bootstrap && \
    make -j$(nproc) && \
    make install && \
    cd / && \
    rm -rf /tmp/cmake-3.27.9 /tmp/cmake-3.27.9.tar.gz

# Set work directory
WORKDIR /build
# Mountpoint for source code
RUN mkdir /clusterscheduler

# Clone the required repositories
#RUN git clone https://github.com/hpc-gridware/clusterscheduler /clusterscheduler

# Define entrypoint for interactions
ENTRYPOINT ["/bin/bash"]
