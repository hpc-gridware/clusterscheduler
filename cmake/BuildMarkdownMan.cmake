function(build_markdown_man section files)
    string(TIMESTAMP DATETIME "%Y-%m-%d")
    foreach(page IN LISTS ${files})
        add_custom_command(
                OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${page}.${section}
                DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${page}.md
                COMMAND ${CMAKE_SOURCE_DIR}/doc/scripts/pandoc_man.sh ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${page} "NONE" ${section} ${CMAKE_PROJECT_VERSION} ${DATETIME}
        )
    endforeach()
endfunction()

function(build_markdown_man_from_template section template files)
    string(TIMESTAMP DATETIME "%Y-%m-%d")
    foreach(page IN LISTS ${files})
        add_custom_command(
                OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${page}.${section}
                DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${page}.md ${CMAKE_CURRENT_SOURCE_DIR}/${template}.md
                COMMAND ${CMAKE_SOURCE_DIR}/doc/scripts/pandoc_man.sh ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${page} ${template} ${section} ${CMAKE_PROJECT_VERSION} ${DATETIME}
        )
    endforeach()
endfunction()

