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

function(architecture_specific_settings)
   get_os_release_info(OS_ID OS_VERSION OS_CODENAME)
   message(STATUS "Build for Version: ${CMAKE_BUILD_ID}")

   message(STATUS "We are on OS: ${OS_ID}; ${OS_VERSION}; ${OS_CODENAME}")

   # Find the OGE architecture string
   execute_process(COMMAND ${CMAKE_SOURCE_DIR}/source/dist/util/arch OUTPUT_VARIABLE SGE_ARCH_RAW)
   string(STRIP ${SGE_ARCH_RAW} SGE_ARCH)
   set(SGE_ARCH ${SGE_ARCH} PARENT_SCOPE)
   message(STATUS "Architecture: ${SGE_ARCH}")

   # Find build and compile architecture and target bits
   set(SGE_COMPILE_ARCH_SCRIPT ${CMAKE_SOURCE_DIR}/source/scripts/compilearch)
   execute_process(COMMAND ${SGE_COMPILE_ARCH_SCRIPT} -b ${SGE_ARCH} OUTPUT_VARIABLE SGE_BUILDARCH_RAW)
   execute_process(COMMAND ${SGE_COMPILE_ARCH_SCRIPT} -c ${SGE_ARCH} OUTPUT_VARIABLE SGE_COMPILEARCH_RAW)
   execute_process(COMMAND ${SGE_COMPILE_ARCH_SCRIPT} -t ${SGE_ARCH} OUTPUT_VARIABLE SGE_TARGETBITS_RAW)
   string(STRIP ${SGE_BUILDARCH_RAW} SGE_BUILDARCH)
   string(STRIP ${SGE_COMPILEARCH_RAW} SGE_COMPILEARCH)
   string(STRIP ${SGE_TARGETBITS_RAW} SGE_TARGETBITS)
   message(STATUS "Buildarch: ${SGE_BUILDARCH}")
   message(STATUS "Compilearch: ${SGE_COMPILEARCH}")
   message(STATUS "Targetbits: ${SGE_TARGETBITS}")

   # directory for installing 3rdparty tools once
   # this allows us to delete the build directory without having to re-build all 3rdparty tools
   set(PROJECT_3RDPARTY_DIR "${PROJECT_3RDPARTY_HOME}/${SGE_ARCH}/${OS_ID}/${OS_VERSION}/${CMAKE_BUILD_TYPE}")

   # spaces in file/directory names break the build
   string(REPLACE " " "_" PROJECT_3RDPARTY_DIR ${PROJECT_3RDPARTY_DIR})
   message(STATUS "3rdparty tools are installed to ${PROJECT_3RDPARTY_DIR}")
   set(PROJECT_3RDPARTY_DIR ${PROJECT_3RDPARTY_DIR} PARENT_SCOPE)
   set(PROJECT_AUTOMAKE_SRC "/usr/share/automake-*/config.*" PARENT_SCOPE)

   # defines for all architectures
   add_compile_definitions(CMAKE_BUILD_ID="${CMAKE_BUILD_ID}" SGE_ARCH_STRING="${SGE_ARCH}" ${SGE_BUILDARCH} ${SGE_COMPILEARCH} ${SGE_TARGETBITS} COMPILE_DC)

   # defines if extensions are enabled
   if (PROJECT_FEATURES MATCHES "gcs-extensions")
      add_compile_definitions(WITH_EXTENSIONS=1)
      message("Build with extensions is enabled")
   endif()

   if (SGE_ARCH MATCHES "lx-riscv64")
      # Linux RiscV
      message(STATUS "We are on Linux: ${SGE_ARCH}")
      # -Wno-deprecated-declarations
      set(CMAKE_C_FLAGS "-Wall -Werror -pedantic" CACHE STRING "" FORCE)
      set(CMAKE_CXX_FLAGS "-Wall -Werror -pedantic" CACHE STRING "" FORCE)

      add_compile_definitions(LINUX _GNU_SOURCE GETHOSTBYNAME_R6 GETHOSTBYADDR_R8 HAS_IN_PORT_T SPOOLING_dynamic __SGE_COMPILE_WITH_GETTEXT__)
      add_compile_options(-fPIC)
      add_link_options(-pthread -rdynamic)

      set(TIRPC_INCLUDES /usr/include/tirpc PARENT_SCOPE)
      set(TIRPC_LIB tirpc PARENT_SCOPE)
      message(STATUS "using libtirpc")

      set(WITH_JEMALLOC OFF PARENT_SCOPE)
      set(WITH_MTMALLOC OFF PARENT_SCOPE)
      set(JNI_ARCH "linux" PARENT_SCOPE)
   elseif (SGE_ARCH MATCHES "lx-.*" OR SGE_ARCH MATCHES "ulx-.*" OR SGE_ARCH MATCHES "xlx-.*")
      # Linux supported/unsupported amd64/x86
      message(STATUS "We are on Linux: ${SGE_ARCH}")
      set(CMAKE_C_FLAGS "-Wall -Werror -pedantic" CACHE STRING "" FORCE)
      set(CMAKE_CXX_FLAGS "-Wall -Werror -pedantic" CACHE STRING "" FORCE)

      # @todo does -fPIC have any disadvantages when not required (only for shared libs)?
      add_compile_options(-fPIC)
      if (CMAKE_BUILD_TYPE STREQUAL "Debug")
         message(STATUS "Doing a debug build")
         # cmake already adds -g, completely disable optimization
         # @todo do we want to add a define like SGE_DEVELOPMENT_BUILD?
         add_compile_options(-O0)
      else ()
         message(STATUS "Doing a release build")
         # cmake already adds -O3 and sets define NDEBUG add_compile_options(-O3)
         if (ENABLE_LTO)
            add_link_options(-flto)
         endif ()
      endif ()
      add_compile_definitions(LINUX _GNU_SOURCE GETHOSTBYNAME_R6 GETHOSTBYADDR_R8
            HAS_IN_PORT_T SPOOLING_dynamic __SGE_COMPILE_WITH_GETTEXT__)
      add_link_options(-pthread -rdynamic)

      set(WITH_MTMALLOC OFF PARENT_SCOPE)

      # specific linux architectures
      if (SGE_TARGETBITS STREQUAL "TARGET_32BIT")
         add_compile_definitions(_FILE_OFFSET_BITS=64)
         # readdir64_r seems to be deprecated
         add_compile_options(-Wno-deprecated-declarations)
      elseif ((OS_ID STREQUAL "raspbian" AND OS_VERSION EQUAL 10)
            OR (OS_ID STREQUAL "tuxedo" AND OS_VERSION EQUAL 22.04)
            OR (OS_ID STREQUAL "ubuntu" AND OS_VERSION EQUAL 22.04)
            OR (OS_ID STREQUAL "rocky" AND OS_VERSION EQUAL 9.4))
         add_compile_options(-Wno-deprecated-declarations)
      endif ()

      # Sanitizers
      # -fsanitize=leak - need to disable custom memory allocators
      # -fsanitize=undefined
      # -fsanitize=address
      if (ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
         message(STATUS "Enabling sanitizers")
         set(WITH_JEMALLOC OFF PARENT_SCOPE)
         add_compile_options("-fno-omit-frame-pointer")
         add_link_options("-fno-omit-frame-pointer")
         add_compile_options("-fsanitize=leak")
         add_link_options("-fsanitize=leak")
         add_compile_options("-fsanitize=undefined")
         add_link_options("-fsanitize=undefined")
         add_compile_options("-fsanitize=address")
         add_link_options("-fsanitize=address")
      endif ()

      # newer Linuxes require libtirp header and library
      if (EXISTS /usr/include/tirpc OR EXISTS /usr/lib64/libtirpc.so)
         set(TIRPC_INCLUDES /usr/include/tirpc PARENT_SCOPE)
         set(TIRPC_LIB tirpc PARENT_SCOPE)
         message(STATUS "using libtirpc")
      elseif (EXISTS /usr/include/ntirpc AND EXISTS /usr/lib/x86_64-linux-gnu/libtirpc.so.3)
         set(TIRPC_INCLUDES /usr/include/ntirpc PARENT_SCOPE)
         set(TIRPC_LIB ntirpc PARENT_SCOPE)
         message(STATUS "using libntirpc")
      endif ()

      if (SGE_ARCH STREQUAL "lx-x86" OR SGE_ARCH STREQUAL "ulx-x86" OR SGE_ARCH STREQUAL "xlx-x86")
         # we need patchelf for setting the run path in the db_* tools
         # but patchelf is not available on CentOS 7 x86
         message(STATUS "Building without Berkeley DB on ${SGE_ARCH}")
         set(WITH_SPOOL_BERKELEYDB OFF PARENT_SCOPE)
         # we need to use a self-compiled gcc/g++/libstdc++ on this platform
         # as the OS packages (CentOS-7) are too old
         # link statically to make sure that the correct libstdc++ is used
         add_compile_options(-static-libstdc++ -static-libgcc)
         add_link_options(-static-libstdc++ -static-libgcc)
      endif ()

      # can't build jemalloc on CentOS 6 - autoconf is too old
      if (SGE_ARCH STREQUAL "xlx-amd64")
         set(WITH_JEMALLOC OFF PARENT_SCOPE)
         # we need to use a self-compiled gcc/g++/libstdc++ on this platform
         # as the OS packages (CentOS-6) are too old
         # link statically to make sure that the correct libstdc++ is used
         add_compile_options(-static-libstdc++ -static-libgcc)
         add_link_options(-static-libstdc++ -static-libgcc)
      endif()

      set(JNI_ARCH "linux" PARENT_SCOPE)
   elseif (SGE_ARCH MATCHES "fbsd-amd64")
      # FreeBSD
      message(STATUS "We are on FreeBSD: ${SGE_ARCH}")
      set(PROJECT_AUTOMAKE_SRC "/usr/local/share/automake-*/config.*" PARENT_SCOPE)
      set(CMAKE_C_FLAGS "-Wall -Werror -pedantic" CACHE STRING "" FORCE)
      set(CMAKE_CXX_FLAGS "-Wall -Werror -pedantic" CACHE STRING "" FORCE)
      add_compile_definitions(FREEBSD GETHOSTBYNAME GETHOSTBYADDR_M SPOOLING_classic)
      add_compile_options(-fPIC)

      set(WITH_JEMALLOC OFF PARENT_SCOPE)
      set(WITH_SPOOL_BERKELEYDB OFF PARENT_SCOPE)
      set(WITH_SPOOL_DYNAMIC OFF PARENT_SCOPE)

      set(JNI_ARCH "freebsd" PARENT_SCOPE)
   elseif (SGE_ARCH MATCHES "sol-.*")
      # Solaris
      message(STATUS "We are on Solaris: ${SGE_ARCH}")
      add_compile_definitions(SOLARIS GETHOSTBYNAME_R5 GETHOSTBYADDR_R7 SPOOLING_dynamic __SGE_COMPILE_WITH_GETTEXT__)
      add_compile_options(-fPIC)
      set(WITH_JEMALLOC OFF PARENT_SCOPE)
      set(WITH_SPOOL_BERKELEYDB OFF PARENT_SCOPE)

      set(JNI_ARCH "solaris" PARENT_SCOPE)
   elseif (SGE_ARCH MATCHES "darwin-arm64")
      # Darwin M1/M2/M2Max/M2Pro (arm64) platform
      message(STATUS "We are on macOS: ${SGE_ARCH}")
      # -Wextra
      set(CMAKE_C_FLAGS "-Wall -Werror -pedantic" CACHE STRING "" FORCE)
      set(CMAKE_CXX_FLAGS "-Wall -Werror -pedantic" CACHE STRING "" FORCE)
      set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures for Mac OS X" FORCE)
      add_compile_definitions(DARWIN DARWIN10 GETHOSTBYNAME GETHOSTBYADDR_M SPOOLING_classic)

      set(WITH_HWLOC OFF PARENT_SCOPE)
      set(WITH_JEMALLOC OFF PARENT_SCOPE)
      set(WITH_SPOOL_BERKELEYDB OFF PARENT_SCOPE)
      set(WITH_SPOOL_DYNAMIC OFF PARENT_SCOPE)

      set(JNI_ARCH "darwin" PARENT_SCOPE)
   else ()
      # unknown platform
      message(WARNING "no arch specific compiler options for ${SGE_ARCH}")
   endif ()
endfunction(architecture_specific_settings)
