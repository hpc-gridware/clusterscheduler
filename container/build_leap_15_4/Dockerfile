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

# Use OpenSUSE Leap 15.4 as the base image
FROM opensuse/leap:15.4

# Update and upgrade system packages
RUN zypper --non-interactive ref && \
    zypper --non-interactive update

# Install development tools pattern
RUN zypper --non-interactive install -t pattern devel_C_C++

# Add GCC repository and install GCC 11
RUN zypper --non-interactive refresh && \
    zypper --non-interactive install gcc11 gcc11-c++ && zypper clean --all

# Update GCC alternatives to use GCC 11
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 11 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 11

# Install essential packages in smaller groups
RUN zypper --non-interactive install git autoconf automake && zypper clean --all
RUN zypper --non-interactive install expect tcl patchelf && zypper clean --all
RUN zypper --non-interactive install gnuplot xterm doxygen graphviz && zypper clean --all
RUN zypper --non-interactive install pandoc cmake openssl && zypper clean --all

# Install each problematic package individually and clean after each
RUN zypper --non-interactive install libopenssl-devel && zypper clean --all
# Skip libdb-5_3-devel for now
# RUN zypper --non-interactive install libdb-5_3-devel && zypper clean --all
RUN zypper --non-interactive install jemalloc-devel jemalloc-devel-static && zypper clean --all
RUN zypper --non-interactive install hwloc && zypper clean --all
RUN zypper --non-interactive install hwloc-devel && zypper clean --all
RUN zypper --non-interactive install gzip && zypper clean --all
RUN zypper --non-interactive install libtirpc-devel && zypper clean --all
RUN zypper --non-interactive install rapidjson-devel && zypper clean --all
RUN zypper --non-interactive install libdb-4_8 libdb-4_8-devel && zypper clean --all

# there is no libudev.so
RUN (cd /usr/lib64 ; ln -s libudev.so.1 libudev.so)

# Set environment variables for C++ compiler
ENV CC=gcc
ENV CXX=g++

# Install a specific version of cmake if it is necessary. Here, install cmake 3.27.9 manually if needed.
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

# Define entrypoint for interactions
ENTRYPOINT ["/bin/bash"]
