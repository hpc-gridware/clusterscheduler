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
        DESCRIPTION "Open Grid Engine Workload Manager"
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

# if build type has not been specified then do a Debug build by default
if(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE
    "Debug"
    CACHE STRING "select build type between Debug and Release" FORCE)
endif()

# 3RD_PARTY OPTIONS
# berkeleydb might not build on all platforms provide a way to disable it @todo
# try updating config.guess and config.sub with a current version
option(WITH_SPOOL_BERKELEYDB  "Enable berkeleydb spooling"                              ON)
option(WITH_SPOOL_DYNAMIC     "Enable dynamic spooling configuration"                   ON)
option(WITH_PLPA              "Enable building with plpa library" ON)
option(WITH_JEMALLOC          "Enable use of the jemalloc memory allocator"             ON)
option(WITH_MTMALLOC          "Enable use of the mtmalloc memory allocator on Solaris"  ON)

# private extensions
set(PROJECT_EXTENSIONS "None" CACHE STRING "directory of private extensions")

# INSTALL OPTIONS
option(INSTALL_SGE_BIN        "Install binaries"                                        ON)
option(INSTALL_SGE_COMMON     "Install common files"                                    ON)
option(INSTALL_SGE_DOC        "Build and install documentation"                         OFF)
option(INSTALL_SGE_TEST       "Install test binaries"                                   ON)

# BUILD OPTIONS
option(ENABLE_LTO             "Enable to add Link Time Optimization in Release Build."  ON)
option(ENABLE_SANITIZERS      "Enable address, leak and undefined behaviour sanitizer"  OFF)

set(PROJECT_3RDPARTY_HOME
        "$ENV{HOME}/3rd_party"
        CACHE STRING "installation directory for 3rdparty tools")
include(cmake/OsReleaseInfo.cmake)
include(cmake/ArchitectureSpecificSettings.cmake)

architecture_specific_settings()

# set rpath in binaries and shared libs
set(CMAKE_INSTALL_RPATH "\$ORIGIN/../../lib/${SGE_ARCH}")

# 3rdparty packages @todo: better install them into
# <base_dir>/<pkg>/<version>/<arch>/<os>/<osversion>/<debug|release>
include(cmake/BuildThirdParty.cmake)
build_third_party("${CMAKE_BINARY_DIR}/3rd_party" ${PROJECT_3RDPARTY_DIR})

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
    ${PROJECT_3RDPARTY_DIR}/include)

if(WITH_JEMALLOC)
  set(SGE_JEMALLOC_LIB "jemalloc")
else()
  set(SGE_JEMALLOC_LIB "")
endif()

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

set(SGE_LIBS ${SGE_JEMALLOC_LIB} pthread dl m)

if(INSTALL_SGE_DOC)
  include(cmake/BuildMarkdownMan.cmake)
  add_subdirectory(doc)
endif()

if(INSTALL_SGE_BIN OR INSTALL_SGE_COMMON OR INSTALL_SGE_TEST)
  add_subdirectory(source)
endif()

if (NOT PROJECT_EXTENSIONS STREQUAL "None")
  add_subdirectory(${PROJECT_EXTENSIONS} "oge-extensions")
endif()

# @todo: instead of library "dl" use ${CMAKE_DL_LIBS}? @todo: add virtual
# targets - for build: similar to --only-core - for install: binaries, common,
# tests, ...
