# source/libs/evm
set(LIBRARY_NAME evm)
set(LIBRARY_SOURCES sge_event_master.c sge_queue_event_master.c)
set(LIBRARY_INCLUDES "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES}
                                                  ${LIBRARY_INCLUDES})
