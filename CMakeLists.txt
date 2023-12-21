# gridengine project root

cmake_minimum_required(VERSION 3.0...3.27.0)

# adapt to your needs or specify -DCMAKE_INSTALL_PREFIX=<path> !!
# CMAKE_INSTALL_PREFIX must be set BEFORE project!!
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX /opt/sge)
endif()

project(
  gridengine
  VERSION 9.0.0
  DESCRIPTION "Open Gridengine Workload Manager"
  HOMEPAGE_URL "https://www.opengridengine.org"
  LANGUAGES C CXX)

message(STATUS "Building project ${PROJECT_NAME} version ${PROJECT_VERSION}")

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#set (CMAKE_C_FLAGS "-arch arm64 -rdynamic -g")
#set (CMAKE_EXE_LINKER_FLAGS "-arch arm64 -flat_namespace -g")

set(CMAKE_BUILD_TYPE
    Debug ON
    CACHE BOOL "select build type between Debug and Release")

# berkeleydb might not build on all platforms provide a way to disable it @todo
# try updating config.guess and config.sub with a current version
set(WITH_SPOOL_BERKELEYDB
    ON
    CACHE BOOL "enable berkeleydb spooling")
set(WITH_SPOOL_DYNAMIC
    ON
    CACHE BOOL "enable dynamic spooling configuration")
set(WITH_PLPA
    ON
    CACHE BOOL "enable building with plpa library")
set(WITH_JEMALLOC
    ON
    CACHE BOOL "enable use of the jemalloc memory allocator")
set(WITH_MTMALLOC
    ON
    CACHE BOOL "enable use of the mtmalloc memory allocator on Solaris")
set(INSTALL_SGE_BIN
    ON
    CACHE BOOL "install binaries")
set(INSTALL_SGE_COMMON
        ON
        CACHE BOOL "install common files")
set(INSTALL_SGE_DOC
    OFF
    CACHE BOOL "build and install documentation")
set(INSTALL_SGE_TEST
    ON
    CACHE BOOL "install test binaries")

include(cmake/OsReleaseInfo.cmake)
include(cmake/ArchitectureSpecificSettings.cmake)

architecture_specific_settings()

# set rpath in binaries and shared libs
set(CMAKE_INSTALL_RPATH "\$ORIGIN/../../lib/${SGE_ARCH}")

# 3rdparty packages @todo: better install them into
# <base_dir>/<pkg>/<version>/<arch>/<os>/<osversion>/<debug|release>
include(cmake/BuildThirdParty.cmake)
set(3RD_PARTY ${CMAKE_BINARY_DIR}/3rd_party)
build_third_party(${3RD_PARTY})

# @todo: we shouldn't need includes - daemons/common, required for err_trace.h
# in libs/sgeobj/sge_binding.h - daemons/execd, required for get_path.h in
# clients/common/show_job.c - daemons/qmaster, required for sge_sched_thread.h
# in libs/sched/sge_urgency.h
set(SGE_INCLUDES
    ${PROJECT_SOURCE_DIR}/source/common
    ${PROJECT_SOURCE_DIR}/source/clients/common
    ${PROJECT_SOURCE_DIR}/source/daemons/common
    ${PROJECT_SOURCE_DIR}/source/daemons/execd
    ${PROJECT_SOURCE_DIR}/source/daemons/qmaster
    ${PROJECT_SOURCE_DIR}/source/libs
    ${3RD_PARTY}/include)

set(SGE_LIBS jemalloc pthread dl m)

if(SGE_ARCH MATCHES "darwin-arm64")
  set(SPOOLING_LIBS spoolloader spoolc_static spool)
else()
  set(SPOOLING_LIBS spoolloader spoold spool)
endif()

if(WITH_PLPA)
  set(SGE_TOPO_LIB "plpa")
else()
  set(SGE_TOPO_LIB "")
endif()

# add_subdirectory(doc)
add_subdirectory(source)

# @todo: instead of library "dl" use ${CMAKE_DL_LIBS}? @todo: add virtual
# targets - for build: similar to --only-core - for install: binaries, common,
# tests, ...
