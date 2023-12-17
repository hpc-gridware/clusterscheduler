function(architecture_specific_settings)
    execute_process(COMMAND ${CMAKE_SOURCE_DIR}/source/dist/util/arch
            OUTPUT_VARIABLE SGE_ARCH_RAW)
    string(STRIP ${SGE_ARCH_RAW} SGE_ARCH)
    set(SGE_ARCH ${SGE_ARCH} PARENT_SCOPE)
    message(STATUS "on platform \"${SGE_ARCH}\"")

    execute_process(COMMAND ${CMAKE_SOURCE_DIR}/source/scripts/compilearch -b ${SGE_ARCH} OUTPUT_VARIABLE SGE_BUILDARCH_RAW)
    string(STRIP ${SGE_BUILDARCH_RAW} SGE_BUILDARCH)

    execute_process(COMMAND ${CMAKE_SOURCE_DIR}/source/scripts/compilearch -c ${SGE_ARCH} OUTPUT_VARIABLE SGE_COMPILEARCH_RAW)
    string(STRIP ${SGE_COMPILEARCH_RAW} SGE_COMPILEARCH)

    execute_process(COMMAND ${CMAKE_SOURCE_DIR}/source/scripts/compilearch -t ${SGE_ARCH} OUTPUT_VARIABLE SGE_TARGETBITS_RAW)
    string(STRIP ${SGE_TARGETBITS_RAW} SGE_TARGETBITS)

    # DARWIN: GETHOSTBYNAME DGETHOSTBYADDR_M
    # SOLARIS: GETHOSTBYNAME_R5 GETHOSTBYADDR_R7
    # only LINUX: HAS_IN_PORT_T and PLPA
    add_compile_definitions(
            SGE_ARCH_STRING="${SGE_ARCH}"
            ${SGE_BUILDARCH}
            ${SGE_COMPILEARCH}
            ${SGE_TARGETBITS}
            USE_POLL
            SPOOLING_dynamic
            NO_JNI
            COMPILE_DC
            __SGE_COMPILE_WITH_GETTEXT__
    )

    if (${SGE_ARCH} STREQUAL lx-amd64 OR
        ${SGE_ARCH} STREQUAL "lx-x86")
        add_compile_options(-Wall -Werror -Wstrict-prototypes -Wno-strict-aliasing)
        add_compile_definitions(
                LINUX
                _GNU_SOURCE
                GETHOSTBYNAME_R6
                GETHOSTBYADDR_R8
                HAS_IN_PORT_T
        )
        if (${WITH_PLPA})
            add_compile_definitions(PLPA)
        endif()
        add_link_options(-pthread -rdynamic)
    elseif (${SGE_ARCH} STREQUAL lx-arm64)
        add_compile_options(-Wall -Werror -Wstrict-prototypes -Wno-strict-aliasing)
        add_compile_definitions(
                LINUX
                _GNU_SOURCE
                GETHOSTBYNAME_R6
                GETHOSTBYADDR_R8
                HAS_IN_PORT_T
        )
        set(WITH_PLPA OFF PARENT_SCOPE)
    elseif (${SGE_ARCH} STREQUAL "sol-amd64")
        add_compile_definitions(
                SOLARIS
                GETHOSTBYNAME_R5
                GETHOSTBYADDR_R7
        )
        set(WITH_PLPA OFF PARENT_SCOPE)
    else()
        message(WARNING "no arch specific compiler options for ${SGE_ARCH}")
    endif()

    # disable berkeleydb on platforms where we cannot (yet) build it
    if (${SGE_ARCH} STREQUAL lx-arm64)
        set(WITH_SPOOL_BERKELEYDB OFF PARENT_SCOPE)
    endif()
endfunction(architecture_specific_settings)