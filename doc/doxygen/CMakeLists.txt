# gridengine/doc/doxygen

# check if Doxygen is installed
find_package(Doxygen)
if (DOXYGEN_FOUND)
   message("Doxygen build started")

   # set input and output files
   set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
   set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

   # copy the Doxygen configuration file to the build location
   configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

   # add the target to the ALL target
   add_custom_target(doc_doxygen ALL
         COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../..
         COMMENT "Generating API documentation with Doxygen"
         VERBATIM)

   # copy the html directory as part of the installation but only if enabled and when in Debug mode
   if (INSTALL_SGE_SRCDOC AND CMAKE_BUILD_TYPE MATCHES "^Debug")
      install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc_doxygen/html DESTINATION doc/doxygen)
   endif ()
else ()
   message("Doxygen need to be installed to generate the doxygen documentation")
endif ()