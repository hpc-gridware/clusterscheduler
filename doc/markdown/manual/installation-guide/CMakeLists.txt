# gridengine/doc/markdown/manual/installation-guide

# build installation guide
build_markdown_manual("installation-guide" "Installation Guide")

# target for building installation guide
add_custom_target(installationguide ALL DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/installation-guide.pdf
#        ${CMAKE_CURRENT_BINARY_DIR}/html
)

# installation
if(INSTALL_SGE_DOC)
   install(FILES ${CMAKE_CURRENT_BINARY_DIR}/installation-guide.pdf DESTINATION doc/pdf)
#   install(FILES ${CMAKE_CURRENT_BINARY_DIR}/html/* DESTINATION doc/html/user-guide)
endif()