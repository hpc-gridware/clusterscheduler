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
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES}
      ${LIBRARY_INCLUDES})

add_executable(test_cull_hash test_cull_hash.cc)
target_include_directories(test_cull_hash PRIVATE "./")
target_link_libraries(test_cull_hash PRIVATE cull uti commlists ${SGE_LIBS})

add_executable(test_cull_list test_cull_list.cc)
target_include_directories(test_cull_list PRIVATE "./")
target_link_libraries(test_cull_list PRIVATE cull uti commlists ${SGE_LIBS})

add_executable(test_cull_observe test_cull_observe.cc)
target_include_directories(test_cull_observe PRIVATE "./")
target_link_libraries(test_cull_observe PRIVATE cull uti commlists ${SGE_LIBS})

add_executable(test_cull_pack test_cull_pack.cc)
target_include_directories(test_cull_pack PRIVATE "./")
target_link_libraries(test_cull_pack PRIVATE cull uti commlists ${SGE_LIBS})

add_executable(test_enumeration test_enumeration.cc)
target_include_directories(test_enumeration PRIVATE "./")
target_link_libraries(test_enumeration PRIVATE cull uti commlists ${SGE_LIBS})

if (INSTALL_SGE_TEST)
   install(TARGETS test_cull_hash DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_list DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_observe DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_pack DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_enumeration DESTINATION testbin/${SGE_ARCH})
endif ()
