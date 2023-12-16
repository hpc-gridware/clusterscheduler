# source/libs/lck
set(LIBRARY_NAME lck)
set(LIBRARY_SOURCES ../uti/sge_lock.c ../uti/sge_lock_fifo.c
                    ../uti/sge_mtutil.c)
set(LIBRARY_INCLUDES "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES}
                                                  ${LIBRARY_INCLUDES})
