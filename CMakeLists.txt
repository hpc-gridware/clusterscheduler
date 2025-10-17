#___INFO__MARK_BEGIN_NEW__
###########################################################################
#  
#  Copyright 2023-2024 HPC-Gridware GmbH
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

# clusterscheduler project root

cmake_minimum_required(VERSION 3.0...3.27.0)

# Get version token from source/libs/sgeobj/ocs_Version.cc
file(STRINGS "source/libs/sgeobj/ocs_Version.cc" VERSION_MAJOR REGEX "^#define OCS_VERSION_MAJOR ")
file(STRINGS "source/libs/sgeobj/ocs_Version.cc" VERSION_MINOR REGEX "^#define OCS_VERSION_MINOR ")
file(STRINGS "source/libs/sgeobj/ocs_Version.cc" VERSION_PATCH REGEX "^#define OCS_VERSION_PATCH ")
file(STRINGS "source/libs/sgeobj/ocs_Version.cc" VERSION_SUFFIX REGEX "^#define OCS_VERSION_SUFFIX ")
string(REGEX REPLACE "#define OCS_VERSION_MAJOR ([0-9]*)" "\\1" VERSION_MAJOR "${VERSION_MAJOR}")
string(REGEX REPLACE "#define OCS_VERSION_MINOR ([0-9]*)" "\\1" VERSION_MINOR "${VERSION_MINOR}")
string(REGEX REPLACE "#define OCS_VERSION_PATCH ([0-9]*)" "\\1" VERSION_PATCH "${VERSION_PATCH}")
string(REGEX REPLACE "#define OCS_VERSION_SUFFIX \"([a-z0-0\-]*)\"" "\\1" VERSION_SUFFIX "${VERSION_SUFFIX}")

# Set the version string and use it to set the version within inst_sge
#set(OCS_VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}${VERSION_SUFFIX}")
#configure_file(
#        "${CMAKE_SOURCE_DIR}/source/dist/inst_sge.in"
#        "${CMAKE_SOURCE_DIR}/source/dist/inst_sge"
#        @ONLY
#)

# Set the version string and use it also for the cmake project
project(
      clusterscheduler
      VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}"
      DESCRIPTION "Open Cluster Scheduler Workload Manager"
      HOMEPAGE_URL "https://www.openclusterscheduler.org"
      LANGUAGES C CXX)


message(STATUS "Building project ${PROJECT_NAME} version ${PROJECT_VERSION} (suffix: ${VERSION_SUFFIX})")

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# specify -DCMAKE_INSTALL_PREFIX=<path> to change the installation directory
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
   message(STATUS "setting default installation directory to /opt/cs")
   set(CMAKE_INSTALL_PREFIX "/opt/cs" CACHE STRING "installation directory" FORCE)
endif ()

# if build type has not been specified then do a Debug build by default
if (NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE
         "Debug"
         CACHE STRING "select build type between Debug and Release" FORCE)
endif ()
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DENABLE_DEBUG_CHECKS")

# 3RD_PARTY OPTIONS
option(WITH_OS_3RDPARTY "Use 3rdparty libraries provided by OS packages" OFF)
option(WITH_SPOOL_BERKELEYDB "Enable berkeleydb spooling" ON)
option(WITH_SPOOL_DYNAMIC "Enable dynamic spooling configuration" ON)
option(WITH_HWLOC "Enable building with hwloc library" ON)
option(WITH_JEMALLOC "Enable use of the jemalloc memory allocator" ON)
option(WITH_OPENSSL "Enable use of the openssl library for CSP mode" OFF)
option(WITH_MTMALLOC "Enable use of the mtmalloc memory allocator on Solaris" ON)
option(WITH_QMAKE "Enable build of qmake" ON)
option(WITH_JNI "Add JNI code for libraries like libdrmaa" ON)
option(WITH_GPERF "Enable profiling code with Google Performance Tools" OFF)
option(WITH_LCOV "Enable code coverage analysis with lcov" OFF)
option(WITH_PYTHON "Enable Python external bindings" OFF)
option(WITH_BOOST "Enable Boost framework" OFF)
option(WITH_MUNGE "Enable Munge authentication" ON)
option(WITH_SYSTEMD "Enable systemd support" OFF)

# private extensions
set(PROJECT_EXTENSIONS "None" CACHE STRING "directory of private extensions")
set(PROJECT_FEATURES "clusterscheduler" CACHE STRING "clusterscheduler or gcs-extensions")

# INSTALL OPTIONS
option(INSTALL_SGE_BIN "Install binaries" ON)
   option(INSTALL_SGE_BIN_CLIENT "Install client binaries" OFF)
   option(INSTALL_SGE_BIN_EXEC "Install execution daemon binaries" OFF)
   option(INSTALL_SGE_BIN_MASTER "Install master daemon binaries" OFF)
option(INSTALL_SGE_COMMON "Install common files" ON)
option(INSTALL_SGE_DOC "Build and install documentation" OFF)
option(INSTALL_SGE_TEST "Install test binaries" ON)
option(INSTALL_SGE_SRCDOC "Build and install source code documentation" OFF)

# set derived install options if user did not set any of them
# might be further refined in ArchitectureSpecificSettings.cmake
if(NOT (INSTALL_SGE_BIN_CLIENT OR INSTALL_SGE_BIN_EXEC OR INSTALL_SGE_BIN_MASTER))
   if(INSTALL_SGE_BIN)
      set(INSTALL_SGE_BIN_CLIENT ON CACHE BOOL "Install client binaries" FORCE)
      set(INSTALL_SGE_BIN_EXEC ON CACHE BOOL "Install execution daemon binaries" FORCE)
      set(INSTALL_SGE_BIN_MASTER ON CACHE BOOL "Install master daemon binaries" FORCE)
   else()
      set(INSTALL_SGE_BIN_CLIENT OFF CACHE BOOL "Install client binaries" FORCE)
      set(INSTALL_SGE_BIN_EXEC OFF CACHE BOOL "Install execution daemon binaries" FORCE)
      set(INSTALL_SGE_BIN_MASTER OFF CACHE BOOL "Install master daemon binaries" FORCE)
   endif()
endif()

# BUILD OPTIONS
option(ENABLE_LTO "Enable to add Link Time Optimization in Release Build." ON)
option(ENABLE_SANITIZERS "Enable address, leak and undefined behaviour sanitizer" OFF)


set(PROJECT_3RDPARTY_HOME
        "$ENV{HOME}/3rd_party"
        CACHE STRING "installation directory for 3rdparty tools")
include(cmake/OsReleaseInfo.cmake)
include(cmake/ArchitectureSpecificSettings.cmake)
include(CTest)

# Catch2 is a header-only test framework, so we can use CPM to install it
#include(cmake/Catch2Config.cmake)

architecture_specific_settings()

# set rpath in binaries and shared libs
set(CMAKE_INSTALL_RPATH "\$ORIGIN/../../lib/${SGE_ARCH}")
#set(CMAKE_BUILD_WITH_INSTALL_RPATH "TRUE")

# 3rdparty packages @todo: better install them into
# <base_dir>/<pkg>/<version>/<arch>/<os>/<osversion>/<debug|release>

include(cmake/BuildThirdParty.cmake)

build_third_party("${CMAKE_BINARY_DIR}/3rd_party" ${PROJECT_3RDPARTY_DIR})

# @todo: we shouldn't need includes - daemons/common, required for err_trace.h
# in libs/sgeobj/sge_binding.h - daemons/execd, required for get_path.h in
# clients/common/show_job.c - daemons/qmaster, required for sge_sched_thread.h
# in libs/sched/sge_urgency.h

if (PROJECT_FEATURES MATCHES "gcs-extensions")
   set(SGE_INCLUDES
         "../../../../${PROJECT_FEATURES}/source/common"
         "../../../../${PROJECT_FEATURES}/source/clients/common"
         "../../../../${PROJECT_FEATURES}/source/daemons/common"
         "../../../../${PROJECT_FEATURES}/source/daemons/execd"
         "../../../../${PROJECT_FEATURES}/source/daemons/qmaster"
         "../../../../${PROJECT_FEATURES}/source/libs"
         ${PROJECT_SOURCE_DIR}/source/common
         ${PROJECT_SOURCE_DIR}/source/clients/common
         ${PROJECT_SOURCE_DIR}/source/daemons/common
         ${PROJECT_SOURCE_DIR}/source/daemons/execd
         ${PROJECT_SOURCE_DIR}/source/daemons/qmaster
         ${PROJECT_SOURCE_DIR}/source/libs
         ${TIRPC_INCLUDES}
         ${PROJECT_3RDPARTY_DIR}/include
         ${CPM_PACKAGE_rapidjson_SOURCE_DIR}/include)
   add_compile_definitions(ADD_GRIDWARE_COPYRIGHT)
else()
   set(SGE_INCLUDES
         ${PROJECT_SOURCE_DIR}/source/common
         ${PROJECT_SOURCE_DIR}/source/clients/common
         ${PROJECT_SOURCE_DIR}/source/daemons/common
         ${PROJECT_SOURCE_DIR}/source/daemons/execd
         ${PROJECT_SOURCE_DIR}/source/daemons/qmaster
         ${PROJECT_SOURCE_DIR}/source/libs
         ${TIRPC_INCLUDES}
         ${PROJECT_3RDPARTY_DIR}/include
         ${CPM_PACKAGE_rapidjson_SOURCE_DIR}/include)
endif()

if (WITH_JEMALLOC)
   set(SGE_JEMALLOC_LIB "jemalloc")
else ()
   set(SGE_JEMALLOC_LIB "")
endif ()

if (WITH_MUNGE)
   add_compile_definitions("OCS_WITH_MUNGE")
endif()

if (WITH_SYSTEMD)
   add_compile_definitions("OCS_WITH_SYSTEMD")
endif()

#if (SGE_ARCH MATCHES "darwin-arm64" OR SGE_ARCH MATCHES "fbsd-amd64")
if (NOT WITH_SPOOL_BERKELEYDB AND NOT WITH_SPOOL_DYNAMIC)
   set(SPOOLING_LIBS spoolloader spoolc_static spool)
else ()
   set(SPOOLING_LIBS spoolloader spoold spool)
endif ()

if (WITH_GPERF)
   # gperftools can be installed on centos with: yum install google-perftools-devel
   set (Gperftools_DIR "${CMAKE_CURRENT_LIST_DIR}/cmake/")
   set (Gperftools_ROOT_DIR "/usr")
   find_package(Gperftools)
   if (Gperftools_FOUND)
      message("Found and enabled build with gperftools")
      add_compile_definitions(WITH_GPERF)
   else()
      # ignore if gperftools are not found so that build with TS does not fail if a build host is missing gperftools
      set(GPERFTOOLS_PROFILER "") # set to empty string to avoid linking against gperftools
      message("Cannot find gperftools although WITH_GPERF is set")
   endif()
else()
   set(GPERFTOOLS_PROFILER "")
endif()

if (WITH_LCOV)
   # enable gcov for code coverage
   list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
   include(cmake/CodeCoverage.cmake)
   message("Found and enabled build with gcov")
   set(COVERAGE_EXCLUDES
           "/opt/rh/*"
           "source/utilbin/*"
   )
   append_coverage_compiler_flags()
#   append_coverage_compiler_flags()
#    Or for specific target:
#   append_coverage_compiler_flags_to_target(YOUR_TARGET_NAME)
endif()

if (WITH_HWLOC)
   if (SGE_ARCH MATCHES "darwin-arm64")
      #set(SGE_TOPO_LIB hwloc CoreFoundation Core)
      set(SGE_TOPO_LIB hwloc)
      set(SGE_TOPO_LIB_PATH "")
   elseif (SGE_ARCH MATCHES "fbsd-amd64")
      set(SGE_TOPO_LIB hwloc udev)
      set(SGE_TOPO_LIB_PATH /usr/local/lib)
   elseif (SGE_ARCH MATCHES "sol-amd64")
      if (SGE_ARCH STREQUAL "osol-amd64")
         set(SGE_TOPO_LIB hwloc picl kstat lgrp)
      else()
         set(SGE_TOPO_LIB hwloc picl kstat lgrp pciaccess)
      endif()
   else ()
      if (OS_ID STREQUAL "ubuntu" AND OS_VERSION EQUAL 24.04)
         set(SGE_TOPO_LIB hwloc udev OpenCL)
      else()
         set(SGE_TOPO_LIB hwloc udev)
      endif()
      set(SGE_TOPO_LIB_PATH "")
      add_compile_definitions(OCS_HWLOC)
   endif ()
   if (SGE_TOPO_LIB_PATH)
      link_directories(/usr/local/lib)
   endif ()
else ()
   set(SGE_TOPO_LIB "")
endif ()

if (WITH_JNI)
   if (DEFINED ENV{JAVA_HOME})
      if (EXISTS "$ENV{JAVA_HOME}/include/jni.h")
         message(STATUS "found $ENV{JAVA_HOME}/include/jni.h, can build JNI part of libdrmaa")
      else()
         message(FATAL_ERROR "JAVA_HOME is set ($ENV{JAVA_HOME}), but JAVA_HOME/include/jni.h does not exist")
         set (WITH_JNI OFF)
      endif()
   else()
      message(FATAL_ERROR "JAVA_HOME not set in environment, cannot build JNI code for libdrmaa")
      set(WITH_JNI OFF)
   endif()
endif()

if (WITH_PYTHON)
   # Ensure that we have Python3 installed
   find_package(Python3 COMPONENTS Interpreter Development)
   if (Python3_FOUND)
      message(STATUS "Python3 found: ${Python3_EXECUTABLE}")
      message(STATUS "Python3 include dirs: ${Python3_INCLUDE_DIRS}")
      message(STATUS "Python3 libraries: ${Python3_LIBRARIES}")
      message(STATUS "Python3 version: ${Python3_VERSION}")
   else()
      message(FATAL_ERROR "Python3 not found")
   endif()

   # We need pybind11 (usually installed via 'pip install pybind11')
   # Find the cmake directory of pybind so that find_package will work
   execute_process(COMMAND python3 -m pybind11 --cmakedir OUTPUT_VARIABLE PYBIND11_CMAKE_DIR)
   string(STRIP ${PYBIND11_CMAKE_DIR} pybind11_DIR)
   find_package(pybind11 REQUIRED)
   if (NOT pybind11_FOUND)
      message(FATAL_ERROR "pybind11 not found")
   else()
      # python3 -m pybind11 --includes
      message(STATUS "pybind11 include dirs: ${pybind11_INCLUDE_DIR}")
   endif ()

   # find the python/pybind11 extension suffix (e.g .cpython-310-x86_64-linux-gnu.so)
   execute_process(COMMAND python3 -c "import sysconfig; print(sysconfig.get_config_var('EXT_SUFFIX'))" OUTPUT_VARIABLE PYBIND11_EXT_SUFFIX)
   string(STRIP ${PYBIND11_EXT_SUFFIX} PYBIND11_EXTENSION_SUFFIX)
   message(STATUS "Python3 ext-suffix: ${PYBIND11_EXTENSION_SUFFIX}")

   # find the pybind11-stubgen tool that allows us to generate pyi-files
   # pyi-files are used by IDEs to provide type hints for the pybind11 module
   #find_program(PYBIND11_STUBGEN pybind11-stubgen)
   find_program(PYBIND11_STUBGEN stubgen)
   if (PYBIND11_STUBGEN)
      message(STATUS "Python stubgen to generate pyi: ${PYBIND11_STUBGEN}")
   endif()

   set(PYTHON_ALL_INCLUDE_DIRS ${Python3_INCLUDE_DIRS} ${pybind11_INCLUDE_DIR})
   set(PYTHON_ALL_LIBRARIES ${Python3_LIBRARIES} util)
   set(PYTHON_EXTENSION_SUFFIX ${PYBIND11_EXTENSION_SUFFIX})
endif ()

if (SGE_ARCH MATCHES "fbsd-amd64")
   set(SGE_LIBS_SHARED pthread ${TIRPC_LIB} kvm ${CMAKE_DL_LIBS} m execinfo)
else ()
   set(SGE_LIBS_SHARED pthread ${TIRPC_LIB} ${CMAKE_DL_LIBS} ${NSL_LIB} m)
endif ()
set(SGE_LIBS ${SGE_JEMALLOC_LIB} ${SGE_LIBS_SHARED})

if (WITH_BOOST)
   find_package(Boost 1.66 COMPONENTS program_options REQUIRED)
   add_compile_definitions(WITH_BOOST)
endif()

if (INSTALL_SGE_DOC)
   include(cmake/BuildMarkdownMan.cmake)
   include(cmake/BuildMarkdownManual.cmake)
   add_subdirectory(doc)
endif ()

if (INSTALL_SGE_BIN OR INSTALL_SGE_BIN_CLIENT OR INSTALL_SGE_BIN_EXEC OR INSTALL_SGE_BIN_MASTER OR INSTALL_SGE_COMMON OR INSTALL_SGE_TEST)
   add_subdirectory(source)
   add_subdirectory(test)
endif ()

if (NOT PROJECT_EXTENSIONS STREQUAL "None")
   add_subdirectory(${PROJECT_EXTENSIONS} "gcs-extensions")
endif ()

# @todo: instead of library "dl" use ${CMAKE_DL_LIBS}? @todo: add virtual
# targets - for build: similar to --only-core - for install: binaries, common,
# tests, ...
