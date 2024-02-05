# source/libs/sched
set(LIBRARY_NAME sched)
set(LIBRARY_SOURCES
      debit.cc
      load_correction.cc
      schedd_message.cc
      schedd_monitor.cc
      sge_complex_schedd.cc
      sge_interactive_sched.cc
      sge_job_schedd.cc
      sge_orders.cc
      sge_pe_schedd.cc
      sge_qeti.cc
      sge_resource_quota_schedd.cc
      sge_resource_utilization.cc
      sge_schedd_text.cc
      sge_select_queue.cc
      sge_serf.cc
      sge_sharetree_printing.cc
      sge_support.cc
      sge_urgency.cc
      sgeee.cc
      sort_hosts.cc
      subordinate_schedd.cc
      suspend_thresholds.cc
      valid_queue_user.cc)
set(LIBRARY_INCLUDES "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES}
      ${LIBRARY_INCLUDES})

add_executable(test_eval_performance test_eval_performance.cc)
target_include_directories(test_eval_performance PRIVATE "./")
target_link_libraries(test_eval_performance PRIVATE sched gdi sgeobj gdi cull comm uti commlists ${SGE_LIBS})

add_executable(test_resource_utilization test_resource_utilization.cc ../../common/sge_mt_init.cc)
target_include_directories(test_resource_utilization PRIVATE "./")
target_link_libraries(test_resource_utilization PRIVATE sched sgeobj gdi cull comm uti commlists ${SGE_LIBS})

add_executable(test_sge_load_formula test_sge_load_formula.cc)
target_include_directories(test_sge_load_formula PRIVATE "./")
target_link_libraries(test_sge_load_formula PRIVATE sched sgeobj gdi cull comm uti commlists ${SGE_LIBS})

if (INSTALL_SGE_TEST)
   install(TARGETS test_eval_performance DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_resource_utilization DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_sge_load_formula DESTINATION testbin/${SGE_ARCH})
endif ()
