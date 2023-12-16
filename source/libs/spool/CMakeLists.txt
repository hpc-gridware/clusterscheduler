# source/libs/spool
set(LIBRARY_NAME spool)
set(LIBRARY_SOURCES sge_dirent.c sge_spooling.c sge_spooling_utilities.c)
set(LIBRARY_INCLUDES "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES}
                                                  ${LIBRARY_INCLUDES})

add_subdirectory(berkeleydb)
# add_subdirectory(classic)
add_subdirectory(dynamic)
add_subdirectory(flatfile)
add_subdirectory(loader)
