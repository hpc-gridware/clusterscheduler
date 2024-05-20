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
    jemalloc-devel \
    hwloc \
    hwloc-devel \
    && dnf clean all


RUN dnf --enablerepo=devel install -y doxygen && dnf clean all 
RUN dnf --enablerepo=epel install -y libdb-devel && dnf clean all

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
WORKDIR /clusterscheduler

# Clone the required repositories (uncomment if you need to clone a specific repo)
#RUN git clone https://github.com/hpc-gridware/clusterscheduler /clusterscheduler

# Define entrypoint for interactions
ENTRYPOINT ["/bin/bash"]
