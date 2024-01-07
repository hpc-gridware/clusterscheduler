function(build_markdown_manual file title)
    string(TIMESTAMP DATETIME "%Y-%m-%d")
    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${file}.pdf
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.md
                    ${CMAKE_CURRENT_SOURCE_DIR}/../titlepage.md
                    ${CMAKE_CURRENT_SOURCE_DIR}/../copyright.md
                    ${CMAKE_CURRENT_SOURCE_DIR}/../definitions.md
                    ${CMAKE_CURRENT_SOURCE_DIR}/../head.tex
            COMMAND ${CMAKE_SOURCE_DIR}/doc/scripts/pandoc_manual.sh ${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_BINARY_DIR} ${file} ${title} ${CMAKE_PROJECT_VERSION} ${DATETIME}
    )
endfunction()
