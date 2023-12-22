# source/libs/spool/flatfile it is the classic spooling lib libspoolc.so
set(LIBRARY_NAME spoolc)
set(LIBRARY_SOURCES sge_flatfile.cc sge_flatfile_obj.cc sge_spooling_flatfile.cc
                    sge_spooling_flatfile_scanner.c ../classic/read_write_job.cc)
set(LIBRARY_INCLUDES "./")

# sge_spooling_flatfile_scanner is generated from flex it will produce compiler
# warnings, disable -Werror
set_source_files_properties(sge_spooling_flatfile_scanner.c PROPERTIES COMPILE_FLAGS -Wno-error)

add_library(${LIBRARY_NAME}_objlib OBJECT ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME}_objlib PUBLIC ${SGE_INCLUDES} ${LIBRARY_INCLUDES})
set_property(TARGET ${LIBRARY_NAME}_objlib PROPERTY POSITION_INDEPENDENT_CODE 1)

add_library(${LIBRARY_NAME}_static STATIC $<TARGET_OBJECTS:${LIBRARY_NAME}_objlib>)
target_include_directories(${LIBRARY_NAME}_static PUBLIC ${SGE_INCLUDES} ${LIBRARY_INCLUDES})

add_library(${LIBRARY_NAME} SHARED $<TARGET_OBJECTS:${LIBRARY_NAME}_objlib>)
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES} ${LIBRARY_INCLUDES})
target_link_libraries(
    ${LIBRARY_NAME}
  PRIVATE
    spool
    sched
    sgeobj
    gdi
    cull
    comm
    uti
    commlists)

if(INSTALL_SGE_BIN)
  install(TARGETS spoolc DESTINATION lib/${SGE_ARCH})
endif()

add_executable(test_flatfile_spool test_flatfile_spool.cc ../../../common/sge_mt_init.cc)
target_include_directories(test_flatfile_spool PRIVATE "./")
target_link_libraries(test_flatfile_spool PRIVATE spool spoolc gdi sgeobj uti commlists)

add_executable(test_sge_spooling_flatfile test_sge_spooling_flatfile.cc ../../../common/sge_mt_init.cc)
target_include_directories(test_sge_spooling_flatfile PRIVATE "./")
target_link_libraries(test_sge_spooling_flatfile PRIVATE spool spoolc gdi sgeobj uti commlists)

if(INSTALL_SGE_TEST)
  install(TARGETS test_flatfile_spool DESTINATION testbin/${SGE_ARCH})
  install(TARGETS test_sge_spooling_flatfile DESTINATION testbin/${SGE_ARCH})
endif()
