# gridengine/doc/markdown/manual/release-notes

# build release notes
build_markdown_manual("release-notes" "Release Notes")

# target for building release notes
add_custom_target(releasenotes ALL DEPENDS
      ${CMAKE_CURRENT_BINARY_DIR}/release-notes.pdf
      #        ${CMAKE_CURRENT_BINARY_DIR}/html
)

# installation
if (INSTALL_SGE_DOC)
   install(FILES ${CMAKE_CURRENT_BINARY_DIR}/release-notes.pdf DESTINATION doc/pdf)
   #   install(FILES ${CMAKE_CURRENT_BINARY_DIR}/html/* DESTINATION doc/html/user-guide)
endif ()