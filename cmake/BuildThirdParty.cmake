#___INFO__MARK_BEGIN_NEW__
###########################################################################
#  
#  Copyright 2023-2025 HPC-Gridware GmbH
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
                        BUILD_COMMAND make clean all
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
                list(APPEND 3rdparty_list 3rd_party_hwloc)
                if(SGE_ARCH STREQUAL "osol-amd64")
                  set(CUSTOM_CFLAGS CFLAGS=-Wno-incompatible-pointer-types)
                endif()
                externalproject_add(
                        3rd_party_hwloc
                        EXCLUDE_FROM_ALL TRUE
                        PREFIX ${3rdparty_build_path}/hwloc
                        INSTALL_DIR ${3rdparty_install_path}
                        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
                        CONFIGURE_COMMAND ./configure
                           --prefix ${3rdparty_install_path}
                           --enable-static --disable-libxml2
                           ${CUSTOM_CFLAGS}
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
            # for openssl we rely on the OS packages only
            message(STATUS "checking for openssl")
            set (OPENSSL_INCLUDE_DIR /usr/include)
            if (SGE_ARCH MATCHES "darwin-.*")
                if (EXISTS "/opt/local/include/openssl")
                    # the darwin MacPorts packages install to /opt/local
                    set (OPENSSL_INCLUDE_DIR /opt/local/include)
                endif()
            endif()
            if (SGE_ARCH MATCHES "fbsd-.*")
               if (EXISTS "/usr/local/include/openssl")
                  # the freebsd openssl3 package installs to /usr/local
                  set (OPENSSL_INCLUDE_DIR /usr/local/include)
               endif()
            endif()
            message(STATUS "searching for openssl in ${OPENSSL_INCLUDE_DIR}")

            if (EXISTS "${OPENSSL_INCLUDE_DIR}/openssl" OR EXISTS "${OPENSSL_INCLUDE_DIR}/openssl3")
                # this is the old CSP mode
                #add_compile_definitions("SECURE")
                #add_compile_definitions("LOAD_OPENSSL")
                if (EXISTS "${OPENSSL_INCLUDE_DIR}/openssl3")
                    # e.g., Rocky-8 with openssl-3.x.x installed
                    # add_compile_definitions("OCS_WITH_OPENSSL3_HEADERS")
                    message(STATUS "Found openssl3 header files")
                    include_directories(BEFORE SYSTEM "${OPENSSL_INCLUDE_DIR}/openssl3")
                    add_compile_definitions("OCS_WITH_OPENSSL")
                else()
                    file(STRINGS "${OPENSSL_INCLUDE_DIR}/openssl/opensslv.h" OPENSSL_MAJOR REGEX "^#.*define OPENSSL_VERSION_MAJOR ")
                    string(REGEX REPLACE "#.*define OPENSSL_VERSION_MAJOR ([0-9]*)" "\\1" OPENSSL_MAJOR "${OPENSSL_MAJOR}")
                    message(STATUS, "Found openssl version: ${OPENSSL_MAJOR}")
                    if (NOT OPENSSL_MAJOR EQUAL "3")
                        message(STATUS "Will not use OpenSSL version != 3")
                        set(WITH_OPENSSL OFF CACHE BOOL "" FORCE)
                    else()
                        add_compile_definitions("OCS_WITH_OPENSSL")
                    endif()
                    # e.g., Ubuntu 24.04 with openssl-3.x being default (usr/include/openssl)
                endif()
            else()
                message(FATAL_ERROR "openssl header files seem not to be installed")
                set(WITH_OPENSSL OFF CACHE BOOL "" FORCE)
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
                COMMAND cp -a ${3rdparty_install_path}/lib/${libname} ${CMAKE_CURRENT_BINARY_DIR}
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

