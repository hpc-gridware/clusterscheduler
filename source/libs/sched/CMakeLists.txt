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


