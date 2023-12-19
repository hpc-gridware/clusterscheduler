# source/libs/spool/dynamic
set(LIBRARY_NAME spoold)
set(LIBRARY_SOURCES sge_spooling_dynamic.c)
set(LIBRARY_INCLUDES "./")

add_library(${LIBRARY_NAME} SHARED ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES}
                                                  ${LIBRARY_INCLUDES})

if(INSTALL_SGE_BIN)
  install(TARGETS spoold DESTINATION lib/${SGE_ARCH})
endif()
