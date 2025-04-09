#!/bin/sh
#___INFO__MARK_BEGIN_NEW__
###########################################################################
#
#  Copyright 2025 HPC-Gridware GmbH
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

# This script can be used to build OpenMPI, e.g version 5.0.7
# usage: build.sh <version> <install_dir>

if [ $# -ne 2 ]; then
    echo "Usage: `basename $0` <version> <install_dir>"
    exit 1
fi

# unset variables which might cause a problem if set
unset MSG

CONFIGURE_OPTIONS=""
# need SGE support, no need for other WML support
CONFIGURE_OPTIONS="$CONFIGURE_OPTIONS --with-sge"
CONFIGURE_OPTIONS="$CONFIGURE_OPTIONS --without-slurm --without-pbs --without-lsf"
# if Fortran is not required
CONFIGURE_OPTIONS="$CONFIGURE_OPTIONS --enable-mpi-fortran=no"

VERSION=$1
MMVERSION=$(echo $VERSION | cut -d. -f1,2)
INSTALL_DIR=$2

BUILD_DIR=$(pwd)
SOURCE_TGZ="https://download.open-mpi.org/release/open-mpi/v$MMVERSION/openmpi-$VERSION.tar.gz"

# download the source
wget $SOURCE_TGZ
tar -xzf openmpi-$VERSION.tar.gz
cd openmpi-$VERSION

# configure and build
./configure --prefix=$INSTALL_DIR 2>&1 | tee configure.log
if [ $? -ne 0 ]; then
    echo "configure failed"
    exit 1
fi
make -j 4 2>&1 | tee make.log
if [ $? -ne 0 ]; then
    echo "make failed"
    exit 1
fi
make install 2>&1 | tee make-install.log
if [ $? -ne 0 ]; then
    echo "make install failed"
    exit 1
fi

# cleanup
cd $BUILD_DIR
rm -rf openmpi-$VERSION
rm -f openmpi-$VERSION.tar.gz

exit 0