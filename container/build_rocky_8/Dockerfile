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

# Use Rocky Linux 8 as the base image
FROM rockylinux:8.9

# Set noninteractive environment to avoid prompts during package installation
ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8

# Install EPEL repository for additional packages
RUN dnf install -y epel-release && dnf update -y

# Install necessary packages

# many tools are only available in the epel-release
RUN dnf install -y epel-release && dnf clean all

# for building cmake
RUN dnf install -y openssl-devel && dnf clean all

# for building binaries
RUN dnf install -y git autoconf automake gcc-toolset-13 systemd-devel patchelf && dnf clean all
RUN dnf install -y libtirpc-devel  && dnf clean all
RUN dnf --enablerepo=powertools install -y munge-devel  && dnf clean all

# building documentation
RUN dnf --enablerepo=devel install -y pandoc texlive texlive-xetex \
    texlive-ucharcat texlive-tcolorbox texlive-fancyvrb texlive-titlesec && dnf clean all

# for building source code (doxygen) documentation
RUN dnf --enablerepo=devel install -y doxygen graphviz && dnf clean all

# for testing
RUN dnf install -y expect tcl gnuplot xterm && dnf clean all

# for cmake -DWITH_OS_3RDPARTY=ON
# which seems not to work on Rocky-8 as there is no static libjemalloc
#RUN dnf install -y jemalloc-devel && dnf clean all
#RUN dnf install -y hwloc hwloc-devel && dnf clean all
#RUN dnf install -y libdb-devel && dnf clean all

# Install a specific version of cmake if it is necessary. Here, install cmake 3.27.9 manually if needed.
# May need to download and compile from source if version is crucial
ADD https://github.com/Kitware/CMake/releases/download/v3.27.9/cmake-3.27.9.tar.gz /tmp/
SHELL [ "/usr/bin/scl", "enable", "gcc-toolset-13" ]
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

# set PATH to gcc-toolset
RUN echo ". /opt/rh/gcc-toolset-13/enable" >> /etc/bashrc

# Clone the required repositories (uncomment if you need to clone a specific repo)
#RUN git clone https://github.com/hpc-gridware/clusterscheduler /clusterscheduler

# Define entrypoint for interactions
ENTRYPOINT ["/bin/bash"]
