# source/libs/spool/berkeleydb
if(${WITH_SPOOL_BERKELEYDB})
  set(LIBRARY_NAME spoolb)
  set(LIBRARY_SOURCES sge_bdb.cc sge_bdb_types.cc sge_spooling_berkeleydb.cc)
  set(LIBRARY_INCLUDES "./")

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
      cull
      comm
      uti
      commlists
      berkeleydb)

  if(INSTALL_SGE_BIN)
    install(TARGETS spoolb DESTINATION lib/${SGE_ARCH})
  endif()
endif()
