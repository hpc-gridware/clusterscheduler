# gridengine/doc

add_subdirectory(markdown)

# Build source code documentation only if enabled and when in debup mode
if (INSTALL_SGE_DOC EQUAL "ON" AND CMAKE_BUILD_TYPE MATCHES "^Debug")
   add_subdirectory(doxygen)
endif()

