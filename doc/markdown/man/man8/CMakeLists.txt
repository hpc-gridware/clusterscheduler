# gridengine/doc/markdown/man/man8

# build all man pages from section 8
set(PAGES sge_execd sge_qmaster sge_shadowd sge_shepherd)
build_markdown_man("8" PAGES)

# target for building all troff man pages from section 8
# @todo is there a better way of listing them? Didn't find an easily comprehensible way to use foreach().
add_custom_target(troffman8 ALL DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/sge_execd.8
        ${CMAKE_CURRENT_BINARY_DIR}/sge_qmaster.8
        ${CMAKE_CURRENT_BINARY_DIR}/sge_shadowd.8
        ${CMAKE_CURRENT_BINARY_DIR}/sge_shepherd.8
)

# installation
if(INSTALL_SGE_DOC)
    foreach(PAGE ${PAGES})
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PAGE}.8 DESTINATION man/man8)
    endforeach ()
endif()
