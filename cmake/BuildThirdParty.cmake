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

# package manager to be used for 3rdparty code which has custom rules (*not*
# cmake) none: use hand written code below we might want to use conan or VCPKG
# instead but - none of them has libplpa (which we need to get rid of) - conan
# has libdb and jemalloc - VCPKG only has an old version of libdb (4.8.30)
#
# for cmake projects we use CPM
set(SGE_PACKAGE_MANAGER none)

function(build_third_party 3rdparty_build_path 3rdparty_install_path)
    if (NOT WITH_OS_3RDPARTY)
        include(cmake/CPM.cmake)
        # cpmaddpackage("gh:Tencent/rapidjson#v1.1.0")
        # OS-distributions of rapidjson-1.1.0 seem to contain patches - the original one doesn't work
        # master branch has the required patches
        cpmaddpackage("gh:Tencent/rapidjson#master")
    endif ()

    set(3rdparty_list "")
    if (${SGE_PACKAGE_MANAGER} STREQUAL none)
        include(ExternalProject)

        # berkeleydb
        if (WITH_SPOOL_BERKELEYDB)
            message(STATUS "adding 3rdparty berkeleydb")
            if (WITH_OS_3RDPARTY)
                find_library(berkeleydb_path NAMES ${CMAKE_SHARED_LIBRARY_PREFIX}db${CMAKE_SHARED_LIBRARY_SUFFIX})
                add_library(berkeleydb SHARED IMPORTED GLOBAL)
                set_target_properties(berkeleydb PROPERTIES IMPORTED_LOCATION
                        ${berkeleydb_path})
            else ()
                if(SGE_ARCH STREQUAL "lx-riscv64")
                  set(CUSTOM_CFLAGS "CFLAGS=-Wno-implicit-int -Wno-incompatible-pointer-types")
                endif()
                # BDB 5.3 predates C99 being enforced: its configure probes write
                # "main() {" without a return type. GCC 14 and later reject that
                # instead of warning, so every probe fails - including the one for
                # POSIX mutexes, after which configure falls back to FCNTL mutexes
                # and aborts with "Support for FCNTL mutexes was removed in BDB 4.8".
                # Measured: gcc 11 (Ubuntu 22.04) builds it, gcc 15 (Ubuntu 26.04)
                # does not, on the same sources - so this is bound to the compiler,
                # not to SGE_ARCH. Older compilers ignore an unknown -Wno-* option
                # unless they emit another diagnostic anyway, so this stays harmless
                # on the CentOS 6/7/8 toolchains.
                if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND
                   CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 14)
                  set(CUSTOM_CFLAGS "CFLAGS=-Wno-implicit-int -Wno-incompatible-pointer-types -Wno-implicit-function-declaration -Wno-return-type")
                endif()
                list(APPEND 3rdparty_list 3rd_party_berkeleydb)
                externalproject_add(
                        3rd_party_berkeleydb
                        EXCLUDE_FROM_ALL TRUE
                        PREFIX ${3rdparty_build_path}/berkeleydb
                        INSTALL_DIR ${3rdparty_install_path}
                        GIT_REPOSITORY https://github.com/Positeral/libdb5.git
                        GIT_TAG master
                        # update config.guess and config.sub with current versions of the installed automake
                        PATCH_COMMAND /bin/sh -c "cp ${PROJECT_AUTOMAKE_SRC} dist"
                        CONFIGURE_COMMAND dist/configure
                           --prefix ${3rdparty_install_path}
                           ${CUSTOM_CFLAGS}
                        BUILD_IN_SOURCE TRUE
                        BUILD_ALWAYS FALSE
                        # -j1 on purpose: make passes its -j down through MAKEFLAGS,
                        # and the testsuite builds this target with -j 128. BDB's
                        # makefile cannot take that - "clean" and "all" then run at
                        # the same time and the link dies with
                        # "mut_tas.lo is not a valid libtool object".
                        BUILD_COMMAND make -j1 clean all
                        INSTALL_COMMAND make install)
                add_library(berkeleydb SHARED IMPORTED GLOBAL)
                set_target_properties(
                        berkeleydb
                        PROPERTIES
                        IMPORTED_LOCATION
                        ${3rdparty_install_path}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}db${CMAKE_SHARED_LIBRARY_SUFFIX}
                )
            endif ()
        endif ()
        # @todo @todo db_* man pages

        # jemalloc
        if (WITH_JEMALLOC)
            message(STATUS "adding 3rdparty jemalloc")
            if (WITH_OS_3RDPARTY)
                find_library(jemalloc_path NAMES ${CMAKE_STATIC_LIBRARY_PREFIX}jemalloc_pic${CMAKE_STATIC_LIBRARY_SUFFIX})
                add_library(jemalloc STATIC IMPORTED GLOBAL)
                set_target_properties(jemalloc PROPERTIES IMPORTED_LOCATION
                        ${jemalloc_path})
            else ()
                list(APPEND 3rdparty_list 3rd_party_jemalloc)
                externalproject_add(
                        3rd_party_jemalloc
                        EXCLUDE_FROM_ALL TRUE
                        PREFIX ${3rdparty_build_path}/jemalloc
                        INSTALL_DIR ${3rdparty_install_path}
                        GIT_REPOSITORY https://github.com/jemalloc/jemalloc.git
                        GIT_TAG 5.3.0
                        CONFIGURE_COMMAND ./autogen.sh --prefix ${3rdparty_install_path}
                        --disable-shared --disable-initial-exec-tls
                        BUILD_IN_SOURCE TRUE
                        BUILD_ALWAYS FALSE
                        BUILD_COMMAND make)
                add_library(jemalloc STATIC IMPORTED GLOBAL)
                set_target_properties(
                        jemalloc
                        PROPERTIES
                        IMPORTED_LOCATION
                        ${3rdparty_install_path}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}jemalloc_pic${CMAKE_STATIC_LIBRARY_SUFFIX}
                )
            endif ()
        endif ()

        if (WITH_HWLOC)
            message(STATUS "adding 3rdparty hwloc")
            if (WITH_OS_3RDPARTY)
                find_library(hwloc_path NAMES ${CMAKE_SHARED_LIBRARY_PREFIX}hwloc${CMAKE_SHARED_LIBRARY_SUFFIX})
                add_library(hwloc SHARED IMPORTED GLOBAL)
                set_target_properties(hwloc PROPERTIES IMPORTED_LOCATION
                        ${hwloc_path})
            else ()
                # A static libhwloc.a carries no dependency information, so every
                # backend hwloc compiles in has to be resolved by whoever links it.
                # OpenCL stays enabled and is linked via SGE_TOPO_LIB; CUDA and NVML
                # are not, because they cannot be linked on the arm64 build host:
                # libcudart.so lives outside the linker path and libnvidia-ml.so
                # (the unversioned development symlink) does not exist there at all.
                # --disable-pci for the same reason, mirrored: libpciaccess links on
                # the arm64 host but not on the amd64 one, so a shared link list
                # cannot satisfy both.
                # GCS calls no hwloc GPU function anyway - verified: not a single
                # hwloc_opencl/cuda/nvml call under source/.
                list(APPEND 3rdparty_list 3rd_party_hwloc)
                externalproject_add(
                        3rd_party_hwloc
                        EXCLUDE_FROM_ALL TRUE
                        PREFIX ${3rdparty_build_path}/hwloc
                        INSTALL_DIR ${3rdparty_install_path}
                        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
                        CONFIGURE_COMMAND ./configure --prefix ${3rdparty_install_path} --enable-static --disable-libxml2 --disable-cuda --disable-nvml --disable-rsmi --disable-levelzero --disable-gl --disable-pci
                        BUILD_IN_SOURCE TRUE
                        BUILD_ALWAYS FALSE
                        BUILD_COMMAND make
                        # put URL last to avoid the "At least one entry of URL is a path (invalid in a list)"-problem
                        URL https://download.open-mpi.org/release/hwloc/v2.10/hwloc-2.10.0.tar.gz)
                add_library(hwloc STATIC IMPORTED GLOBAL)
                set_target_properties(
                        hwloc
                        PROPERTIES
                        IMPORTED_LOCATION
                        ${3rdparty_install_path}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}hwloc${CMAKE_STATIC_LIBRARY_SUFFIX}
                )
            endif ()
        endif ()

        if (WITH_OPENSSL)
            message(STATUS "adding 3rdparty openssl")
            if (WITH_OS_3RDPARTY)
                if (EXISTS "/usr/include/openssl")
                    add_compile_definitions("SECURE")
                    add_compile_definitions("LOAD_OPENSSL")
                else()
                    message(FATAL_ERROR "openssl header files seem not to be installed")
                endif()
            else ()
                message(FATAL_ERROR "can build with openssl only with os packages")
            endif()
        endif()

    endif ()

    # add a target containing all 3rdparty libs which need to be built once
    message(STATUS "We are building the following 3rdparty libraries: ${3rdparty_list}")
    add_custom_target(3rdparty DEPENDS ${3rdparty_list})
endfunction(build_third_party)

# copy thirdparty files from their installation directory
# to the current build directory
# make sure that they contain the correct rpath
function(install_third_party_bin 3rdparty_install_path target_dir files)
    foreach (file IN LISTS ${files})
        add_custom_command(
                OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${file}
                COMMAND cp ${3rdparty_install_path}/bin/${file} ${CMAKE_CURRENT_BINARY_DIR}
                COMMAND chmod 755 ${CMAKE_CURRENT_BINARY_DIR}/${file}
                COMMAND patchelf --set-rpath ${CMAKE_INSTALL_RPATH} ${CMAKE_CURRENT_BINARY_DIR}/${file}
                VERBATIM
        )
        message(STATUS "adding 3rdparty bin ${file}")
        add_custom_target(${file}
                ALL
                DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${file}
        )
        install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/${file} DESTINATION utilbin/${SGE_ARCH})
    endforeach ()
endfunction()

function(install_third_party_lib 3rdparty_install_path target_dir files)
    foreach (file IN LISTS ${files})
        set(libname "${CMAKE_SHARED_LIBRARY_PREFIX}${file}${CMAKE_SHARED_LIBRARY_SUFFIX}")
        add_custom_command(
                OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${libname}
                # -d keeps the symlinks (libdb-5.so points at libdb-5.3.so), but no -a/-p:
                # since coreutils 9 cp reports a failure to carry the ACLs over from the
                # NFS source as an error, and the build dies. Timestamps are enough here,
                # the mode comes from the source and install(PROGRAMS) sets it again.
                COMMAND cp -d --preserve=timestamps --no-preserve=xattr,context ${3rdparty_install_path}/lib/${libname} ${CMAKE_CURRENT_BINARY_DIR}
                VERBATIM
        )
        message(STATUS "adding 3rdparty lib ${libname}")
        add_custom_target(${libname}
                ALL
                DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${libname}
        )
        install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/${libname} DESTINATION lib/${SGE_ARCH})
    endforeach ()
endfunction()

