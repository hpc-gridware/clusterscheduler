#!/bin/sh

# @todo I cannot stop it via CTRL-C

DO_SOURCE_BASHRC=0
DO_GCS=0
DO_BUILD=1
DO_BUILD_3RDPARTY=1
DO_BUILD_DOC=0
DO_TARGZ=0
DO_UNITTEST=0
DO_INSTALL=0
DO_TEST=0

SGE_ROOT="/opt/cs"

if [ -z $GITHUB_ACTIONS ]; then
   # running in a local docker container
   OCS_DIR="/clusterscheduler/clusterscheduler"
   GCS_DIR="/clusterscheduler/gcs-extensions"
   PKG_DIR="/clusterscheduler/packages"
else
   # running in github actions
   OCS_DIR="$RUNNER_WORKSPACE/clusterscheduler"
   GCS_DIR="$RUNNER_WORKSPACE/gcs-extensions"
   PKG_DIR="$RUNNER_WORKSPACE/clusterscheduler/packages"
fi

Usage()
{
   echo "Usage: `basename $0` [options]"
   echo "   Options:"
   echo "   -h             print this help"
   echo "   -bashrc        source /root/.bashrc"
   echo "   -doc           also build documentation"
   echo "   -gcs           build Cluster Scheduler with Gridware extensions"
   echo "   -pkg           create tar.gz packages"
   echo "   -no-3rdparty   do not build 3rdparty code"
   exit $1
}

ParseArgs()
{
   for i in "$@"; do
      case $i in
         -h)
            Usage 0
            ;;
         -bashrc)
            DO_SOURCE_BASHRC=1
            ;;
         -doc)
            DO_BUILD_DOC=1
            ;;
         -gcs)
            DO_GCS=1
            ;;
         -pkg)
            DO_TARGZ=1
            ;;
         -no-3rdparty)
            DO_BUILD_3RDPARTY=0
            ;;
          *)
            Usage 1
            ;;
      esac
   done
}

ParseArgs "$@"

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

   if [ $DO_BUILD_DOC -ne 0 ]; then
      CMAKE_OPTIONS="$CMAKE_OPTIONS -DINSTALL_SGE_DOC=ON"
   fi

   cd /build
   cmake $CMAKE_OPTIONS
   if [ $? -ne 0 ]; then
      echo "cmake failed" >&2
      exit 1
   fi

   if [ $DO_BUILD_3RDPARTY -ne 0 ]; then
      make -j$(nproc) 3rdparty
      if [ $? -ne 0 ]; then
         echo "make 3rdparty failed" >&2
         exit 1
      fi
   fi

   make -j$(nproc)
   if [ $? -ne 0 ]; then
      echo "make failed" >&2
      exit 1
   fi

   make -j$(nproc) install
   if [ $? -ne 0 ]; then
      echo "make install failed" >&2
      exit 1
   fi
fi

if [ $DO_TARGZ -ne 0 ]; then
   echo "Creating Packages"

   if [ ! -d $PKG_DIR ]; then
      mkdir $PKG_DIR
   fi

   MK_DIST="$OCS_DIR/source/scripts/mk_dist"
   VERSION=`( cd /opt/cs && ./inst_sge -v | cut -d " " -f 3 )`
   if [ $DO_GCS -ne 0 ]; then
      PRODUCT="gcs"
   else
      PRODUCT="ocs"
   fi

   PKG_TYPES="-bin -common"
   if [ $DO_BUILD_DOC -ne 0 ]; then
      PKG_TYPES="$PKG_TYPES -doc"
   fi

   ARCH=`/opt/cs/util/arch`

   $MK_DIST -vdir $SGE_ROOT -product $PRODUCT -version $VERSION -basedir $PKG_DIR $PKG_TYPES $ARCH
fi

if [ $DO_UNITTEST -ne 0 ]; then
   echo "Running Unit Tests not yet implemented"
fi
if [ $DO_INSTALL -ne 0 ]; then
   echo "Running SC installation not yet implemented"
fi
if [ $DO_TEST -ne 0 ]; then
   echo "Running Integration Tests not yet implemented"
fi
