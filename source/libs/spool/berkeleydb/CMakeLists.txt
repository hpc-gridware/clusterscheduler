# source/libs/spool/berkeleydb
if (${WITH_SPOOL_BERKELEYDB})
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

   if (INSTALL_SGE_BIN)
      install(TARGETS spoolb DESTINATION lib/${SGE_ARCH})

      # we copy Berkeleydb utilities to utilbin
      # they need to contain rpath with ORIGIN
      set(BDB_PROGS db_deadlock db_dump db_load db_printlog db_recover db_stat db_upgrade db_verify)
      install_third_party_bin(${PROJECT_3RDPARTY_DIR} "utilbin" BDB_PROGS)

      # and we need the libdb installed in $SGE_ROOT/lib/<arch>
      set(BDB_LIBS db-5.3 db-5 db)
      install_third_party_lib(${PROJECT_3RDPARTY_DIR} "lib" BDB_LIBS)
   endif ()
endif ()
