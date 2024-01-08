# package manager to be used for 3rdparty code which has custom rules (*not*
# cmake) none: use hand written code below we might want to use conan or VCPKG
# instead but - none of them has libplpa (which we need to get rid of) - conan
# has libdb and jemalloc - VCPKG only has an old version of libdb (4.8.30)
#
# for cmake projects we use CPM
set(SGE_PACKAGE_MANAGER none)

function(build_third_party 3rdparty_build_path 3rdparty_install_path)
  include(cmake/CPM.cmake)

  cpmaddpackage("gh:Tencent/rapidjson#master")

  set(3rdparty_list "")
  if(${SGE_PACKAGE_MANAGER} STREQUAL none)
    include(ExternalProject)

    # berkeleydb
    if(${WITH_SPOOL_BERKELEYDB})
      message(STATUS "adding 3rdparty berkeleydb")
      list(APPEND 3rdparty_list 3rd_party_berkeleydb)
      ExternalProject_Add(
        3rd_party_berkeleydb
        EXCLUDE_FROM_ALL TRUE
        PREFIX ${3rdparty_build_path}/berkeleydb
        INSTALL_DIR ${3rdparty_install_path}
        GIT_REPOSITORY https://github.com/Positeral/libdb5.git
        GIT_TAG master
	# update config.guess and config.sub with current versions of the installed automake
	PATCH_COMMAND /bin/sh -c "cp /usr/share/automake-*/config.* dist"
        CONFIGURE_COMMAND dist/configure --prefix ${3rdparty_install_path}
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
    endif()
    # @todo @todo db_* man pages

    # jemalloc @todo for all platforms? Only lx-amd64?
    message(STATUS "adding 3rdparty jemalloc")
    list(APPEND 3rdparty_list 3rd_party_jemalloc)
    ExternalProject_Add(
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

    # plpa
    if(${WITH_PLPA})
      message(STATUS "adding 3rdparty plpa")
      list(APPEND 3rdparty_list 3rd_party_plpa)
      ExternalProject_Add(
        3rd_party_plpa
        EXCLUDE_FROM_ALL TRUE
        PREFIX ${3rdparty_build_path}/plpa
        INSTALL_DIR ${3rdparty_install_path}
        URL https://download.open-mpi.org/release/plpa/v1.3/plpa-1.3.2.tar.gz
	# update config.guess and config.sub with current versions of the installed automake
	PATCH_COMMAND /bin/sh -c "cp /usr/share/automake-*/config.* config"
        CONFIGURE_COMMAND ./configure --prefix ${3rdparty_install_path}
                          --disable-shared
        BUILD_IN_SOURCE TRUE
        BUILD_ALWAYS FALSE
        BUILD_COMMAND make)
      add_library(plpa STATIC IMPORTED GLOBAL)
      set_target_properties(
        plpa
        PROPERTIES
          IMPORTED_LOCATION
          ${3rdparty_install_path}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}plpa${CMAKE_STATIC_LIBRARY_SUFFIX}
      )
    endif()
  endif()

  # add a target containing all 3rdparty libs which need to be built once
  message(
    STATUS "We are building the following 3rdparty libraries: ${3rdparty_list}")
  add_custom_target(3rdparty DEPENDS ${3rdparty_list})
endfunction(build_third_party)

# copy thirdparty files from their installation directory
# to the current build directory
# make sure that they contain the correct rpath
function(install_third_party_bin 3rdparty_install_path target_dir files)
  foreach(file IN LISTS ${files})
    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${file}
            COMMAND cp ${3rdparty_install_path}/bin/${file} ${CMAKE_CURRENT_BINARY_DIR}
            COMMAND chmod 755 ${CMAKE_CURRENT_BINARY_DIR}/${file}
            COMMAND patchelf --set-rpath ${CMAKE_INSTALL_RPATH} ${CMAKE_CURRENT_BINARY_DIR}/${file}
            VERBATIM
    )
    add_custom_target(${file}
            ALL
            DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${file}
    )
    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/${file} DESTINATION utilbin/${SGE_ARCH})
  endforeach()
endfunction()

function(install_third_party_lib 3rdparty_install_path target_dir files)
  foreach(file IN LISTS ${files})
    set(libname ${CMAKE_SHARED_LIBRARY_PREFIX}${file}${CMAKE_SHARED_LIBRARY_SUFFIX})
    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${libname}
            COMMAND cp -a ${3rdparty_install_path}/lib/${libname} ${CMAKE_CURRENT_BINARY_DIR}
            VERBATIM
    )
    add_custom_target(${libname}
            ALL
            DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${libname}
    )
    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/${libname} DESTINATION lib/${SGE_ARCH})
  endforeach()
endfunction()

