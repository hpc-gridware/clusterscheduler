# gridengine project root

cmake_minimum_required(VERSION 3.0...3.27.0)

# adapt to your needs or specify -DCMAKE_INSTALL_PREFIX=<path>
# !! CMAKE_INSTALL_PREFIX must be set BEFORE project!!
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX /opt/sge)
endif()

project(gridengine
  VERSION 9.0.0
  DESCRIPTION "Open Gridengine Workload Manager"
  HOMEPAGE_URL "https://www.opengridengine.org"
  LANGUAGES C CXX)

message(STATUS "Building ${PROJECT_NAME} ${PROJECT_VERSION}")

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/ArchitectureSpecificSettings.cmake)
architecture_specific_settings()

# set rpath in binaries and shared libs
set(CMAKE_INSTALL_RPATH "\$ORIGIN/../../lib/${SGE_ARCH}")

# 3rdparty packages
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

set(SPOOLING_LIBS spoolloader spoold spool)

add_compile_options(
  # @todo does -fPIC have any disadvantages when not required (only for shared libs)?
  -fPIC -O2 -Wstrict-prototypes)

# add_subdirectory(doc)
add_subdirectory(source)

# @todo: instead of library "dl" use ${CMAKE_DL_LIBS}?
# @todo: add virtual targets
#        - for build: similar to --only-core
#        - for install: binaries, common, tests, ...