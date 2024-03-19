# source/daemons/qmaster
add_executable(
      sge_qmaster
      configuration_qmaster.cc
      job_exit.cc
      job_report_qmaster.cc
      oge_MirrorDataStore.cc
      oge_MirrorReaderDataStore.cc
      oge_MirrorListenerDataStore.cc
      oge_ReportingFileWriter.cc
      oge_thread_mirror.cc
      qmaster_to_execd.cc
      reschedule.cc
      sched_conf_qmaster.cc
      setup_qmaster.cc
      sge_advance_reservation_qmaster.cc
      sge_c_gdi.cc
      sge_c_report.cc
      sge_calendar_qmaster.cc
      sge_centry_qmaster.cc
      sge_ckpt_qmaster.cc
      sge_cqueue_qmaster.cc
      sge_follow.cc
      sge_give_jobs.cc
      sge_hgroup_qmaster.cc
      sge_host_qmaster.cc
      sge_job_enforce_limit.cc
      sge_job_qmaster.cc
      sge_job_verify.cc
      sge_manop_qmaster.cc
      sge_pe_qmaster.cc
      sge_persistence_qmaster.cc
      sge_qinstance_qmaster.cc
      sge_qmaster_heartbeat.cc
      sge_qmaster_main.cc
      sge_qmaster_process_message.cc
      sge_qmaster_threads.cc
      sge_qmaster_timed_event.cc
      sge_qmod_qmaster.cc
      sge_reporting_qmaster.cc
      sge_resource_quota_qmaster.cc
      sge_rusage.cc
      sge_sched_job_category.cc
      sge_sched_order.cc
      sge_sched_prepare_data.cc
      sge_sched_process_events.cc
      sge_sched_thread.cc
      sge_sched_thread_rsmap.cc
      sge_sharetree_qmaster.cc
      sge_subordinate_qmaster.cc
      sge_task_depend.cc
      sge_thread_event_master.cc
      sge_thread_listener.cc
      sge_thread_main.cc
      sge_thread_scheduler.cc
      sge_thread_signaler.cc
      sge_thread_timer.cc
      sge_thread_worker.cc
      sge_userprj_qmaster.cc
      sge_userset_qmaster.cc
      sge_utility_qmaster.cc
      ../../../../${PROJECT_FEATURES}/source/daemons/qmaster/TestClass.cc
      ../../common/sig_handlers.cc
      ../../common/sge_options.cc
      ../../common/usage.cc
      ../../common/execution_states.cc
)

target_include_directories(sge_qmaster PUBLIC "./" ${SGE_INCLUDES})

target_link_libraries(
      sge_qmaster
      PUBLIC daemonscommon
      sched
      evm
      mir
      evc
      gdi
      sgeobj
      cull
      comm
      commlists
      uti
      ${SGE_TOPO_LIB}
      PRIVATE
      ${SGE_LIBS}
      ${SPOOLING_LIBS}
)

if (INSTALL_SGE_BIN)
   install(TARGETS sge_qmaster DESTINATION bin/${SGE_ARCH})
endif ()

add_executable(test_sge_qmaster_timed_event test_sge_qmaster_timed_event.cc sge_qmaster_timed_event.cc)
target_include_directories(test_sge_qmaster_timed_event PRIVATE "./")
target_link_libraries(test_sge_qmaster_timed_event PRIVATE sched mir evc sgeobj gdi cull comm uti commlists ${SGE_LIBS})

add_executable(test_sge_calendar test_sge_calendar.cc
        oge_ReportingFileWriter.cc
      sge_calendar_qmaster.cc sge_utility_qmaster.cc sge_advance_reservation_qmaster.cc
      sge_qmaster_timed_event.cc sge_reporting_qmaster.cc sge_rusage.cc sge_qinstance_qmaster.cc
      sge_qmod_qmaster.cc sge_job_qmaster.cc sge_give_jobs.cc sge_subordinate_qmaster.cc
      reschedule.cc sge_persistence_qmaster.cc job_exit.cc sge_task_depend.cc sge_host_qmaster.cc
      sge_job_enforce_limit.cc qmaster_to_execd.cc sge_cqueue_qmaster.cc sge_userprj_qmaster.cc
      configuration_qmaster.cc sge_userset_qmaster.cc sge_resource_quota_qmaster.cc sge_sharetree_qmaster.cc
      sge_c_gdi.cc sge_pe_qmaster.cc sge_ckpt_qmaster.cc sge_hgroup_qmaster.cc sge_thread_scheduler.cc
      sge_qmaster_threads.cc sge_sched_job_category.cc sge_sched_process_events.cc sge_sched_prepare_data.cc
      sge_sched_thread.cc sge_sched_thread_rsmap.cc sge_sched_order.cc sge_thread_main.cc sched_conf_qmaster.cc sge_centry_qmaster.cc
      sge_follow.cc sge_job_verify.cc setup_qmaster.cc sge_manop_qmaster.cc sge_thread_signaler.cc
      ../../common/execution_states.cc ../../common/sge_options.cc ../../common/sig_handlers.cc ../../common/usage.cc)
target_include_directories(test_sge_calendar PRIVATE "./")
target_link_libraries(test_sge_calendar PRIVATE daemonscommon ${SPOOLING_LIBS}
      sched mir evc evm sgeobj gdi cull comm uti commlists ${SGE_TOPO_LIB} ${SGE_LIBS})

if (INSTALL_SGE_TEST)
   install(TARGETS test_sge_qmaster_timed_event DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_sge_calendar DESTINATION testbin/${SGE_ARCH})
endif ()
