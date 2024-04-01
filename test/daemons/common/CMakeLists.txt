# test/daemons/common

add_executable(test_common_category test_common_category.cc)
target_include_directories(test_common_category PRIVATE "./")
target_link_libraries(test_common_category PRIVATE daemonscommon sched sgeobj cull comm uti commlists ${SGE_LIBS})
add_test(NAME test_common_category COMMAND test_common_category)

add_executable(test_common_heartbeat test_common_heartbeat.cc)
target_include_directories(test_common_heartbeat PRIVATE "./")
target_link_libraries(test_common_heartbeat PRIVATE daemonscommon uti commlists ${SGE_LIBS})
add_test(NAME test_common_heartbeat COMMAND test_common_heartbeat)

if (INSTALL_SGE_TEST)
   install(TARGETS test_common_category DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_common_heartbeat DESTINATION testbin/${SGE_ARCH})
endif ()
