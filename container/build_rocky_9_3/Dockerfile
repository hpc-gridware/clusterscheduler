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

# Use Rocky Linux 9.3 as the base image
FROM rockylinux:9.3

# Set noninteractive environment to avoid prompts during package installation
ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8

# Install EPEL repository for additional packages
RUN dnf install -y epel-release && dnf update -y

# Install necessary packages
RUN dnf install -y \
    git \
    autoconf \
    automake \
    gcc \
    gcc-c++ \
    make \
    man-pages \
    libudev-devel \
    expect \
    tcl \
    patchelf \
    gnuplot \
    xterm \
    graphviz \
    pandoc \
    cmake \
    openssl \
    openssl-devel \
    && dnf clean all

RUN dnf --enablerepo=devel install -y libtirpc-devel && dnf clean all
RUN dnf --enablerepo=devel install -y doxygen && dnf clean all 

# for cmake -DWITH_OS_3RDPARTY=ON
# which does not work on Rocky-9 as there is no static libjemalloc
#RUN dnf install -y jemalloc-devel && dnf clean all
#RUN dnf install -y hwloc hwloc-devel && dnf clean all
#RUN dnf --enablerepo=epel install -y libdb-devel && dnf clean all

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

# Clone the required repositories (uncomment if you need to clone a specific repo)
#RUN git clone https://github.com/hpc-gridware/clusterscheduler /clusterscheduler

# Define entrypoint for interactions
ENTRYPOINT ["/bin/bash"]
