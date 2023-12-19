# source/libs/spool/flatfile it is the classic spooling lib libspoolc.so
set(LIBRARY_NAME spoolc)

set(LIBRARY_SOURCES sge_flatfile.c sge_flatfile_obj.c sge_spooling_flatfile.c
                    sge_spooling_flatfile_scanner.c ../classic/read_write_job.c)

# sge_spooling_flatfile_scanner is generated from flex it will produce compiler
# warnings, disable -Werror
set_source_files_properties(sge_spooling_flatfile_scanner.c
                            PROPERTIES COMPILE_FLAGS -Wno-error)

set(LIBRARY_INCLUDES "./")
add_library(${LIBRARY_NAME} SHARED ${LIBRARY_SOURCES})

target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES}
                                                  ${LIBRARY_INCLUDES})

target_link_libraries(
  ${LIBRARY_NAME}
  spool
  sched
  sgeobj
  cull
  comm
  uti
  commlists)

if(INSTALL_SGE_BIN)
  install(TARGETS spoolc DESTINATION lib/${SGE_ARCH})
endif()
