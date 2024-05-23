#!/bin/sh

# @todo I cannot stop it via CTRL-C

DO_SOURCE_BASHRC=0
DO_GCS=0
DO_BUILD=1
DO_TARGZ=1
DO_UNITTEST=1
DO_INSTALL=1
DO_TEST=1


if [ -z $GITHUB_ACTIONS ]; then
   # running in a local docker container
   OCS_DIR="/clusterscheduler/clusterscheduler"
   GCS_DIR="/clusterscheduler/gcs-extensions"
else
   # running in github actions
   OCS_DIR="$RUNNER_WORKSPACE/clusterscheduler"
   GCS_DIR="$RUNNER_WORKSPACE/gcs-extensions"
fi

Usage()
{
   echo "Usage: `basename $0` [options]"
   echo "   Options:"
   echo "   -h          print this help"
   echo "   -bashrc     source /root/.bashrc"
   echo "   -gcs        build Cluster Scheduler with Gridware extensions"
   exit $1
}

ParseArgs()
{
   for i in $*; do
      case $i in
         -h)
            Usage 0
            ;;
         -gcs)
            DO_GCS=1
            ;;
         -bashrc)
            DO_SOURCE_BASHRC=1
            ;;
          *)
            Usage 1
            ;;
      esac
   done
}

ParseArgs $*

if [ $DO_SOURCE_BASHRC -ne 0 ]; then
   . /root/.bashrc
fi

CMAKE_OPTIONS="-S $OCS_DIR"

if [ ! -d $OCS_DIR ]; then 
   echo "directory $OCS_DIR does not exist"
   exit 1
fi

if [ $DO_GCS -ne 0 ]; then
   if [ ! -d $GCS_DIR ]; then 
      echo "directory $GCS_DIR does not exist"
      exit 1
   fi

   CMAKE_OPTIONS="$CMAKE_OPTIONS -DPROJECT_EXTENSIONS=$GCS_DIR -DPROJECT_FEATURES=gcs-extensions"
fi

echo $CMAKE_OPTIONS

if [ $DO_BUILD -ne 0 ]; then
   echo "Building Cluster Scheduler"
   cd /build
   cmake $CMAKE_OPTIONS
   if [ $? -ne 0 ]; then
      echo "cmake failed" >&2
      exit 1
   fi

   make 3rdparty
   if [ $? -ne 0 ]; then
      echo "make 3rdparty failed" >&2
      exit 1
   fi

   make
   if [ $? -ne 0 ]; then
      echo "make failed" >&2
      exit 1
   fi

   make install
   if [ $? -ne 0 ]; then
      echo "make install failed" >&2
      exit 1
   fi
fi
