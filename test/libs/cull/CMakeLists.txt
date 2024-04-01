# test/libs/cull

add_executable(test_cull_hash test_cull_hash.cc)
target_include_directories(test_cull_hash PRIVATE "./")
target_link_libraries(test_cull_hash PRIVATE cull uti commlists ${SGE_LIBS})
add_test(NAME test_cull_hash COMMAND test_cull_hash)

add_executable(test_cull_list test_cull_list.cc)
target_include_directories(test_cull_list PRIVATE "./")
target_link_libraries(test_cull_list PRIVATE cull uti commlists ${SGE_LIBS})
add_test(NAME test_cull_list COMMAND test_cull_list)

add_executable(test_cull_observe test_cull_observe.cc)
target_include_directories(test_cull_observe PRIVATE "./")
target_link_libraries(test_cull_observe PRIVATE cull uti commlists ${SGE_LIBS})
add_test(NAME test_cull_observe COMMAND test_cull_observe)

add_executable(test_cull_pack test_cull_pack.cc)
target_include_directories(test_cull_pack PRIVATE "./")
target_link_libraries(test_cull_pack PRIVATE cull uti commlists ${SGE_LIBS})
add_test(NAME test_cull_pack COMMAND test_cull_pack)

add_executable(test_cull_enumeration test_cull_enumeration.cc)
target_include_directories(test_cull_enumeration PRIVATE "./")
target_link_libraries(test_cull_enumeration PRIVATE cull uti commlists ${SGE_LIBS})
add_test(NAME test_cull_enumeration COMMAND test_cull_enumeration)

if (INSTALL_SGE_TEST)
   install(TARGETS test_cull_hash DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_list DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_observe DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_pack DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_enumeration DESTINATION testbin/${SGE_ARCH})
endif ()
