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

# This script can be used to build MPICH-4.x.x
# usage: build.sh <version> <install_dir>

if [ $# -ne 2 ]; then
    echo "Usage: `basename $0` <version> <install_dir>"
    exit 1
fi

CONFIGURE_OPTIONS=""
# if Fortran is not required - comment out if you need it
CONFIGURE_OPTIONS="$CONFIGURE_OPTIONS --disable-fortran --disable-f77 --disable-f90 --disable-f08"
# if C++ is not required - comment out if you need it
CONFIGURE_OPTIONS="$CONFIGURE_OPTIONS --disable-cxx"
# just for testing without having a high speed network
CONFIGURE_OPTIONS="$CONFIGURE_OPTIONS --with-device=ch4:ofi"

VERSION=$1
INSTALL_DIR=$2

BUILD_DIR=$(pwd)
SOURCE_TGZ="https://www.mpich.org/static/downloads/$VERSION/mpich-$VERSION.tar.gz"

# download the source
wget $SOURCE_TGZ
tar -xzf mpich-$VERSION.tar.gz
cd mpich-$VERSION

# create the install directory
mkdir -p $INSTALL_DIR
if [ $? -ne 0 ]; then
   exit 1
fi

# configure and build
./configure $CONFIGURE_OPTIONS --prefix=$INSTALL_DIR 2>&1 | tee configure.log
exit_code=$?
cp configure.log $INSTALL_DIR
if [ $exit_code -ne 0 ]; then
    echo "configure failed"
    exit 1
fi

make -j 4 2>&1 | tee make.log
exit_code=$?
cp make.log $INSTALL_DIR
if [ $exit_code -ne 0 ]; then
    echo "make failed"
    exit 1
fi
make install 2>&1 | tee make-install.log
exit_code=$?
cp make-install.log $INSTALL_DIR
if [ $exit_code -ne 0 ]; then
    echo "make install failed"
    exit 1
fi

# cleanup
cd $BUILD_DIR
rm -rf mpich-$VERSION
rm -f mpich-$VERSION.tar.gz

exit 0
