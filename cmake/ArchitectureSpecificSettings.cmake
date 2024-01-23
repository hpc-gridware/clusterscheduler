function(architecture_specific_settings)
  get_os_release_info(OS_ID OS_VERSION OS_CODENAME)
  message(STATUS "We are on OS: ${OS_ID}; ${OS_VERSION}; ${OS_CODENAME}")

  execute_process(COMMAND ${CMAKE_SOURCE_DIR}/source/dist/util/arch
                  OUTPUT_VARIABLE SGE_ARCH_RAW)
  string(STRIP ${SGE_ARCH_RAW} SGE_ARCH)
  set(SGE_ARCH
      ${SGE_ARCH}
      PARENT_SCOPE)

  execute_process(COMMAND ${CMAKE_SOURCE_DIR}/source/scripts/compilearch -b
                          ${SGE_ARCH} OUTPUT_VARIABLE SGE_BUILDARCH_RAW)
  string(STRIP ${SGE_BUILDARCH_RAW} SGE_BUILDARCH)

  execute_process(COMMAND ${CMAKE_SOURCE_DIR}/source/scripts/compilearch -c
                          ${SGE_ARCH} OUTPUT_VARIABLE SGE_COMPILEARCH_RAW)
  string(STRIP ${SGE_COMPILEARCH_RAW} SGE_COMPILEARCH)

  execute_process(COMMAND ${CMAKE_SOURCE_DIR}/source/scripts/compilearch -t
                          ${SGE_ARCH} OUTPUT_VARIABLE SGE_TARGETBITS_RAW)
  string(STRIP ${SGE_TARGETBITS_RAW} SGE_TARGETBITS)

  # directory for installing 3rdparty tools once
  # this allows us to delete the build directory without having to re-build all 3rdparty tools
  set(PROJECT_3RDPARTY_DIR "${PROJECT_3RDPARTY_HOME}/${SGE_ARCH}/${OS_ID}/${OS_VERSION}/${CMAKE_BUILD_TYPE}")
  # spaces in file/directory names break the build
  string(REPLACE " " "_" PROJECT_3RDPARTY_DIR ${PROJECT_3RDPARTY_DIR})
  message(STATUS "3rdparty tools are installed to ${PROJECT_3RDPARTY_DIR}")
  set(PROJECT_3RDPARTY_DIR ${PROJECT_3RDPARTY_DIR} PARENT_SCOPE)
  set(PROJECT_AUTOMAKE_SRC "/usr/share/automake-*/config.*" PARENT_SCOPE)

  # DARWIN: GETHOSTBYNAME DGETHOSTBYADDR_M
  # SOLARIS: GETHOSTBYNAME_R5 GETHOSTBYADDR_R7 
  # only LINUX: HAS_IN_PORT_T
  add_compile_definitions(
    SGE_ARCH_STRING="${SGE_ARCH}"
    ${SGE_BUILDARCH}
    ${SGE_COMPILEARCH}
    ${SGE_TARGETBITS}
    USE_POLL
    COMPILE_DC)

  if(SGE_ARCH MATCHES "lx-.*")
    # Linux
    message(STATUS "We are on Linux: ${SGE_ARCH}")
    set(CMAKE_C_FLAGS "-Wall -Werror -Wno-deprecated-declarations -Wstrict-prototypes -Wno-strict-aliasing -pedantic" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS "-Wall -Werror -Wno-deprecated-declarations -Wno-write-strings -Wno-literal-suffix -pedantic" CACHE STRING "" FORCE)

    # @todo does -fPIC have any disadvantages when not required (only for shared
    # libs)?
    add_compile_options(-fPIC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      message(STATUS "Doing a debug build")
      # cmake already adds -g, completely disable optimization @todo do we want
      # to add a define like SGE_DEVELOPMENT_BUILD?
      add_compile_options(-O0)
    else()
      message(STATUS "Doing a release build")
      # cmake already adds -O3 and sets define NDEBUG add_compile_options(-O3)
      if (ENABLE_LTO)
        add_link_options(-flto)
      endif()
    endif()
    add_compile_definitions(LINUX _GNU_SOURCE GETHOSTBYNAME_R6 GETHOSTBYADDR_R8
                            HAS_IN_PORT_T SPOOLING_dynamic __SGE_COMPILE_WITH_GETTEXT__)
    add_link_options(-pthread -rdynamic)

    set(WITH_MTMALLOC OFF PARENT_SCOPE)
    set(WITH_JEMALLOC ON PARENT_SCOPE)

    # specific linux architectures
    if(SGE_TARGETBITS STREQUAL "TARGET_32BIT")
      add_compile_definitions(_FILE_OFFSET_BITS=64)
      # readdir64_r seems to be deprecated
      add_compile_options(-Wno-deprecated-declarations)
    elseif((OS_ID STREQUAL "raspbian" AND OS_VERSION EQUAL 10) OR
           (OS_ID STREQUAL "tuxedo" AND OS_VERSION EQUAL 22.04) OR
           (OS_ID STREQUAL "ubuntu" AND OS_VERSION EQUAL 22.04)
            )
      add_compile_options(-Wno-deprecated-declarations)
    endif()

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
    endif()

    # newer Linuxes require libtirp
    if (EXISTS /usr/include/tirpc)
        set (TIRPC_INCLUDES /usr/include/tirpc PARENT_SCOPE)
        set (TIRPC_LIB tirpc PARENT_SCOPE)
    endif()

    if (SGE_ARCH STREQUAL "lx-x86")
      # we need patchelf for setting the run path in the db_* tools
      # but patchelf is not available on CentOS 7 x86
      message(STATUS "Building without Berkeley DB on lx-x86")
      set (WITH_SPOOL_BERKELEYDB OFF PARENT_SCOPE)
    endif()

  elseif(SGE_ARCH MATCHES "fbsd-amd64")
    # FreeBSD
    message(STATUS "We are on FreeBSD: ${SGE_ARCH}")
    set(PROJECT_AUTOMAKE_SRC "/usr/local/share/automake-*/config.*" PARENT_SCOPE)
    set(CMAKE_C_FLAGS "-Wall -Werror -Wno-reserved-user-defined-literal -Wno-writable-strings -Wno-format-pedantic -Wno-unused-const-variable -Wno-pointer-to-int-cast -pedantic" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS "-Wall -Werror -Wno-reserved-user-defined-literal -Wno-writable-strings -Wno-format-pedantic -Wno-unused-const-variable -Wno-pointer-to-int-cast -pedantic" CACHE STRING "" FORCE)
    add_compile_definitions(FREEBSD GETHOSTBYNAME GETHOSTBYADDR_M SPOOLING_classic)
    add_compile_options(-fPIC)

    set(WITH_JEMALLOC OFF PARENT_SCOPE)
    set(WITH_SPOOL_BERKELEYDB OFF PARENT_SCOPE)
    set(WITH_SPOOL_DYNAMIC OFF PARENT_SCOPE)
    set(WITH_PLPA OFF PARENT_SCOPE)
  elseif(SGE_ARCH MATCHES "sol-.*")
    # Solaris
    message(STATUS "We are on Solaris: ${SGE_ARCH}")
    add_compile_definitions(SOLARIS GETHOSTBYNAME_R5 GETHOSTBYADDR_R7 SPOOLING_dynamic __SGE_COMPILE_WITH_GETTEXT__)

    set(WITH_JEMALLOC OFF PARENT_SCOPE)

  elseif(SGE_ARCH MATCHES "darwin-arm64")
    # Darwin M1/M2/M2Max/M2Pro (arm64) platform
    message(STATUS "We are on macOS: ${SGE_ARCH}")
    set(CMAKE_C_FLAGS "-Wall -Werror -Wno-unused-const-variable -Wno-format-pedantic -Wno-pointer-to-int-cast -Wno-strict-prototypes -Wno-reserved-user-defined-literal -Wno-deprecated-declarations -Wno-strict-aliasing -pedantic" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS "-Wall -Werror -Wno-unused-const-variable -Wno-format-pedantic -Wno-pointer-to-int-cast -Wno-strict-prototypes -Wno-reserved-user-defined-literal -Wno-deprecated-declarations -Wno-write-strings -pedantic" CACHE STRING "" FORCE)
    SET(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures for Mac OS X" FORCE)
    add_compile_definitions(DARWIN DARWIN10 GETHOSTBYNAME GETHOSTBYADDR_M SPOOLING_classic)
    set(WITH_JEMALLOC OFF PARENT_SCOPE)
    set(WITH_PLPA OFF PARENT_SCOPE)
    set(WITH_SPOOL_BERKELEYDB OFF PARENT_SCOPE)
    set(WITH_SPOOL_DYNAMIC OFF PARENT_SCOPE)
  else()
    # unknown platform
    message(WARNING "no arch specific compiler options for ${SGE_ARCH}")
  endif()

  if(WITH_HWLOC)
    add_compile_definitions(OGE_HWLOC)
  endif()
endfunction(architecture_specific_settings)
