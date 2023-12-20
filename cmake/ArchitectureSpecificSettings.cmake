function(architecture_specific_settings)
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

  # DARWIN: GETHOSTBYNAME DGETHOSTBYADDR_M SOLARIS: GETHOSTBYNAME_R5
  # GETHOSTBYADDR_R7 only LINUX: HAS_IN_PORT_T and PLPA
  add_compile_definitions(
    SGE_ARCH_STRING="${SGE_ARCH}"
    ${SGE_BUILDARCH}
    ${SGE_COMPILEARCH}
    ${SGE_TARGETBITS}
    USE_POLL
    SPOOLING_dynamic
    NO_JNI
    COMPILE_DC
    __SGE_COMPILE_WITH_GETTEXT__)

  # Linux
  if(SGE_ARCH MATCHES "lx-.*")
    message(STATUS "We are on Linux: ${SGE_ARCH}")
    add_compile_options(-Wall -Werror -Wstrict-prototypes -Wno-strict-aliasing
                        -Wstrict-prototypes -pedantic)

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
    endif()
    add_compile_definitions(LINUX _GNU_SOURCE GETHOSTBYNAME_R6 GETHOSTBYADDR_R8
                            HAS_IN_PORT_T)
    add_link_options(-pthread -rdynamic)

    set(WITH_MTMALLOC
        OFF
        PARENT_SCOPE)

    # specific linux architectures
    if(SGE_TARGETBITS STREQUAL "TARGET_32BIT")
      add_compile_definitions(_FILE_OFFSET_BITS=64)
      # readdir64_r seems to be deprecated
      add_compile_options(-Wno-deprecated-declarations)
    endif()


    # Solaris
  elseif(SGE_ARCH MATCHES "sol-.*")
    message(STATUS "We are on Solaris: ${SGE_ARCH}")
    add_compile_definitions(SOLARIS GETHOSTBYNAME_R5 GETHOSTBYADDR_R7)
    set(WITH_PLPA
        OFF
        PARENT_SCOPE)
    set(WITH_JEMALLOC
        OFF
        PARENT_SCOPE)

    # unknown platform
  else()
    message(WARNING "no arch specific compiler options for ${SGE_ARCH}")
  endif()

  if(WITH_PLPA)
    add_compile_definitions(PLPA)
  endif()
endfunction(architecture_specific_settings)
