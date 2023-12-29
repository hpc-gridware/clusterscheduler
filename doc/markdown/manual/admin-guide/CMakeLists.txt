# gridengine/doc/markdown/manual/admin-guide

# build user guide
build_markdown_manual("admin-guide" "Administrator Guide")

# target for building admin guide
add_custom_target(adminguide ALL DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/admin-guide.pdf
#        ${CMAKE_CURRENT_BINARY_DIR}/html
)

# installation
if(INSTALL_SGE_DOC)
   install(FILES ${CMAKE_CURRENT_BINARY_DIR}/admin-guide.pdf DESTINATION doc/pdf)
#   install(FILES ${CMAKE_CURRENT_BINARY_DIR}/html/* DESTINATION doc/html/user-guide)
endif()