# source/libs/japi
set(LIBRARY_NAME japi)
set(LIBRARY_SOURCES japi.cc)
set(LIBRARY_INCLUDES "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES}
      ${LIBRARY_INCLUDES})

set(DRMAA_LIB_NAME drmaa)
set(DRMAA_LIB_SOURCES drmaa.cc ../../common/parse_job_cull.cc ../../common/parse_qsub.cc)
set(DRMAA_LIB_INCLUDES "./")

add_library(${DRMAA_LIB_NAME} SHARED ${DRMAA_LIB_SOURCES})
target_include_directories(${DRMAA_LIB_NAME} PUBLIC ${SGE_INCLUDES}
      ${DRMAA_LIB_INCLUDES})
target_link_libraries(${DRMAA_LIB_NAME} PRIVATE
      clientscommon
      japi
      evc
      gdi
      sgeobj
      cull
      comm
      commlists
      uti
)

# test binaries
add_executable(test_drmaa test_drmaa.cc)
target_include_directories(test_drmaa PRIVATE "./")
# @todo need to get rid of the plpa dependency (SGE_TOPO_LIB) !!
target_link_libraries(test_drmaa PRIVATE drmaa ${SGE_TOPO_LIB} ${SGE_LIBS})

add_executable(test_drmaa_issue1768 test_drmaa_issue1768.cc)
target_include_directories(test_drmaa_issue1768 PRIVATE "./")
target_link_libraries(test_drmaa_issue1768 PRIVATE drmaa ${SGE_TOPO_LIB} ${SGE_LIBS})

add_executable(test_drmaa_issue1832 test_drmaa_issue1832.cc)
target_include_directories(test_drmaa_issue1832 PRIVATE "./")
target_link_libraries(test_drmaa_issue1832 PRIVATE drmaa ${SGE_TOPO_LIB} ${SGE_LIBS})

add_executable(test_drmaa_issue1892 test_drmaa_issue1892.cc)
target_include_directories(test_drmaa_issue1892 PRIVATE "./")
target_link_libraries(test_drmaa_issue1892 PRIVATE drmaa ${SGE_TOPO_LIB} ${SGE_LIBS})

add_executable(test_drmaa_mcpu test_drmaa_mcpu.cc)
target_include_directories(test_drmaa_mcpu PRIVATE "./")
target_link_libraries(test_drmaa_mcpu PRIVATE drmaa ${SGE_TOPO_LIB} ${SGE_LIBS})

add_executable(test_drmaa_no_bin test_drmaa_no_bin.cc)
target_include_directories(test_drmaa_no_bin PRIVATE "./")
target_link_libraries(test_drmaa_no_bin PRIVATE drmaa ${SGE_TOPO_LIB} ${SGE_LIBS})

add_executable(test_drmaa_perf test_drmaa_perf.cc)
target_include_directories(test_drmaa_perf PRIVATE "./")
target_link_libraries(test_drmaa_perf PRIVATE drmaa ${SGE_TOPO_LIB} ${SGE_LIBS})

add_executable(test_drmaa_sync test_drmaa_sync.cc)
target_include_directories(test_drmaa_sync PRIVATE "./")
target_link_libraries(test_drmaa_sync PRIVATE drmaa ${SGE_TOPO_LIB} ${SGE_LIBS})

if (INSTALL_SGE_COMMON)
   install(FILES drmaa.h DESTINATION include)

   install(
         FILES example.c
         howto/howto1_1.c
         howto/howto1.c
         howto/howto2_1.c
         howto/howto2.c
         howto/howto3_1.c
         howto/howto3_2.c
         howto/howto3.c
         howto/howto4.c
         howto/howto5.c
         howto/howto6.c
         DESTINATION examples/drmaa)
endif ()

if (INSTALL_SGE_BIN)
   install(TARGETS drmaa DESTINATION lib/${SGE_ARCH})
endif ()

if (INSTALL_SGE_TEST)
   install(TARGETS test_drmaa DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_drmaa_issue1768 DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_drmaa_issue1832 DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_drmaa_issue1892 DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_drmaa_mcpu DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_drmaa_no_bin DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_drmaa_perf DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_drmaa_sync DESTINATION testbin/${SGE_ARCH})
endif ()
