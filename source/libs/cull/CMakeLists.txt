# source/libs/cull
set(LIBRARY_NAME cull)
set(LIBRARY_SOURCES
    cull_db.c
    cull_dump_scan.c
    cull_file.c
    cull_hash.c
    cull_lerrno.c
    cull_list.c
    cull_multitype.c
    cull_observe.c
    cull_pack.c
    cull_parse.c
    cull_sort.c
    cull_state.c
    cull_tree.c
    cull_what.c
    cull_what_print.c
    cull_where.c
    pack.c)
set(LIBRARY_INCLUDES "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES}
                                                  ${LIBRARY_INCLUDES})
