# source/libs/spool
set(LIBRARY_NAME spool)
set(LIBRARY_SOURCES sge_dirent.cc sge_spooling.cc sge_spooling_utilities.cc)
set(LIBRARY_INCLUDES "./")

# OGE-199 makes use of readdir_r/readdir64_r which is deprecated for Linux only
set_source_files_properties(sge_dirent.cc PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES}
      ${LIBRARY_INCLUDES})




add_subdirectory(berkeleydb)
# add_subdirectory(classic)
add_subdirectory(dynamic)
add_subdirectory(flatfile)
add_subdirectory(loader)
