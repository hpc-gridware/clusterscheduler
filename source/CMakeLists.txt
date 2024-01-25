# source
add_subdirectory(3rdparty)
add_subdirectory(clients)
add_subdirectory(common)
add_subdirectory(daemons)
add_subdirectory(dist)
add_subdirectory(libs)
add_subdirectory(security)
add_subdirectory(utilbin)

if (INSTALL_SGE_COMMON)
   install(
         DIRECTORY scripts/dtrace
         DESTINATION .
         USE_SOURCE_PERMISSIONS)
endif ()
