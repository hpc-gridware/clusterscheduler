# gridengine/doc/markdown/manual/user-guide

# build user guide
build_markdown_manual("user-guide" "User Guide")

# target for building user guide
add_custom_target(userguide ALL DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/user-guide.pdf
#        ${CMAKE_CURRENT_BINARY_DIR}/html
)

# installation
if(INSTALL_SGE_DOC)
   install(FILES ${CMAKE_CURRENT_BINARY_DIR}/user-guide.pdf DESTINATION doc/pdf)
#   install(FILES ${CMAKE_CURRENT_BINARY_DIR}/html/* DESTINATION doc/html/user-guide)
endif()