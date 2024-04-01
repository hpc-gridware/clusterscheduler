# test/libs/sched

add_executable(test_sched_eval_performance test_sched_eval_performance.cc)
target_include_directories(test_sched_eval_performance PRIVATE "./")
target_link_libraries(test_sched_eval_performance PRIVATE sched gdi sgeobj gdi cull comm uti commlists ${SGE_LIBS})
add_test(NAME test_sched_eval_performance COMMAND test_sched_eval_performance)

add_executable(test_sched_resource_utilization test_sched_resource_utilization.cc)
target_include_directories(test_sched_resource_utilization PRIVATE "./")
target_link_libraries(test_sched_resource_utilization PRIVATE sched sgeobj gdi cull comm uti commlists ${SGE_LIBS})
add_test(NAME test_sched_resource_utilization COMMAND test_sched_resource_utilization)

add_executable(test_sched_load_formula test_sched_load_formula.cc)
target_include_directories(test_sched_load_formula PRIVATE "./")
target_link_libraries(test_sched_load_formula PRIVATE sched sgeobj gdi cull comm uti commlists ${SGE_LIBS})
add_test(NAME test_sched_load_formula COMMAND test_sched_load_formula)

if (INSTALL_SGE_TEST)
   install(TARGETS test_sched_eval_performance DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_sched_resource_utilization DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_sched_load_formula DESTINATION testbin/${SGE_ARCH})
endif ()