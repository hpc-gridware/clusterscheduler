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

# usage: build.sh [suffix]
# suffix can be used to differentiate between different architetures, e.g.
# build.sh lx-amd64

# path to the MPI implementation
if [ -z "$MPIR_HOME" ]; then
    echo "MPIR_HOME is not set"
    exit 1
fi

BINARY="testmpi"
if [ $# -gt 0 ]; then
    BINARY="$BINARY-$1"
fi

PATH=$MPIR_HOME/bin:$PATH
export PATH

CFLAGS=""
LFLAGS=""
# need additional options for Intel MPI
type mpiicc >/dev/null 2>&1
if [ $? -eq 0 ]; then
    CFLAGS="$CFLAGS -I$MPIR_HOME/include"
    LFLAGS="$LFLAGS -L$MPIR_HOME/lib"
fi

mpicc $CFLAGS $LFLAGS -o $BINARY $SGE_ROOT/mpi/examples/testmpi.c -lm