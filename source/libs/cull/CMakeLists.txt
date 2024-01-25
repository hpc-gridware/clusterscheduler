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

add_executable(test_cull_hash test_cull_hash.c)
target_include_directories(test_cull_hash PRIVATE "./")
target_link_libraries(test_cull_hash PRIVATE cull uti commlists ${SGE_LIBS})

add_executable(test_cull_list test_cull_list.c)
target_include_directories(test_cull_list PRIVATE "./")
target_link_libraries(test_cull_list PRIVATE cull uti commlists ${SGE_LIBS})

add_executable(test_cull_observe test_cull_observe.c)
target_include_directories(test_cull_observe PRIVATE "./")
target_link_libraries(test_cull_observe PRIVATE cull uti commlists ${SGE_LIBS})

add_executable(test_cull_pack test_cull_pack.c)
target_include_directories(test_cull_pack PRIVATE "./")
target_link_libraries(test_cull_pack PRIVATE cull uti commlists ${SGE_LIBS})

add_executable(test_enumeration test_enumeration.c)
target_include_directories(test_enumeration PRIVATE "./")
target_link_libraries(test_enumeration PRIVATE cull uti commlists ${SGE_LIBS})

if (INSTALL_SGE_TEST)
   install(TARGETS test_cull_hash DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_list DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_observe DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_pack DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_enumeration DESTINATION testbin/${SGE_ARCH})
endif ()
