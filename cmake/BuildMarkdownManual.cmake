function(build_markdown_manual file title)
   string(TIMESTAMP DATETIME "%Y-%m-%d")
   add_custom_command(
         OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${file}.pdf
         DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.md
         ${CMAKE_SOURCE_DIR}/../gridengine/doc/markdown/manual/titlepage.md
         ${CMAKE_SOURCE_DIR}/../gridengine/doc/markdown/manual/copyright.md
         ${CMAKE_SOURCE_DIR}/../gridengine/doc/markdown/manual/typographic_conventions.md
         ${CMAKE_SOURCE_DIR}/../gridengine/doc/markdown/manual/head.tex
         ${CMAKE_SOURCE_DIR}/../gridengine/doc/scripts/pandoc_manual.sh
         COMMAND ${CMAKE_SOURCE_DIR}/doc/scripts/pandoc_manual.sh ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
         ${CMAKE_CURRENT_BINARY_DIR} ${file} ${title} ${CMAKE_PROJECT_VERSION} ${DATETIME}
   )
endfunction()
