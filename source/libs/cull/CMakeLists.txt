# source/libs/cull
set(LIBRARY_NAME cull)
set(LIBRARY_SOURCES
        cull_db.cc
        cull_dump_scan.cc
        cull_file.cc
        cull_hash.cc
        cull_lerrno.cc
        cull_list.cc
        cull_multitype.cc
        cull_observe.cc
        cull_pack.cc
        cull_parse.cc
        cull_sort.cc
        cull_state.cc
        cull_tree.cc
        cull_what.cc
        cull_what_print.cc
        cull_where.cc
        pack.cc)
set(LIBRARY_INCLUDES "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES} ${LIBRARY_INCLUDES})
