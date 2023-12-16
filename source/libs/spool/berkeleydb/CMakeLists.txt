# source/libs/spool/berkeleydb
set(LIBRARY_NAME spoolb)
set(LIBRARY_SOURCES sge_bdb.c sge_bdb_types.c sge_spooling_berkeleydb.c)
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
  commlists
  berkeleydb)

install(TARGETS spoolb DESTINATION lib/${SGE_ARCH})
