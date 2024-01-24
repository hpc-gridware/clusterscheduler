# gridengine/doc/markdown/manual/development-guide

# build installation guide
build_markdown_manual("development-guide" "Development Guide")

# target for building installation guide
add_custom_target(developmentguide ALL DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/development-guide.pdf
)

# installation
if (INSTALL_SGE_DOC)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/development-guide.pdf DESTINATION doc/pdf)
endif ()