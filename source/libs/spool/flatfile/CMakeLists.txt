# source/libs/spool/flatfile it is the classic spooling lib libspoolc.so
set(LIBRARY_NAME spoolc)
set(LIBRARY_SOURCES
        sge_flatfile.cc
        sge_flatfile_obj.cc
        sge_flatfile_obj_rsmap.cc
        sge_spooling_flatfile.cc
        sge_spooling_flatfile_scanner.cc
        ../classic/read_write_job.cc)
set(LIBRARY_INCLUDES "./")

# sge_spooling_flatfile_scanner is generated from flex it will produce compiler that we need to disable
set_source_files_properties(sge_spooling_flatfile_scanner.cc PROPERTIES COMPILE_FLAGS -Wno-error)

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
      commlists ${SGE_LIBS})

if (INSTALL_SGE_BIN)
   install(TARGETS spoolc DESTINATION lib/${SGE_ARCH})
endif ()

