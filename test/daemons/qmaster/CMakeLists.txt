#___INFO__MARK_BEGIN_NEW__
###########################################################################
#  
#  Copyright 2024 HPC-Gridware GmbH
#  
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#  
#      http://www.apache.org/licenses/LICENSE-2.0
#  
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#  
###########################################################################
#___INFO__MARK_END_NEW__

# test/daemons/qmaster

add_executable(test_qmaster_timed_event
      test_qmaster_timed_event.cc
      ../../../source/daemons/qmaster/sge_qmaster_timed_event.cc)
target_include_directories(test_qmaster_timed_event PRIVATE "./")
target_link_libraries(test_qmaster_timed_event PRIVATE sched mir evc sgeobj gdi cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_qmaster_timed_event COMMAND test_qmaster_timed_event)

add_executable(test_qmaster_calendar
      test_qmaster_calendar.cc
        ../../../source/daemons/qmaster/ocs_ReportingFileWriter.cc
        ../../../source/daemons/qmaster/ocs_BaseAccountingFileWriter.cc
        ../../../source/daemons/qmaster/ocs_BaseReportingFileWriter.cc
        ../../../source/daemons/qmaster/ocs_JsonAccountingFileWriter.cc
        ../../../source/daemons/qmaster/ocs_JsonReportingFileWriter.cc
        ../../../source/daemons/qmaster/ocs_MonitoringFileWriter.cc
      ../../../source/daemons/qmaster/sge_calendar_qmaster.cc
      ../../../source/daemons/qmaster/sge_utility_qmaster.cc
      ../../../source/daemons/qmaster/sge_advance_reservation_qmaster.cc
      ../../../source/daemons/qmaster/sge_qmaster_timed_event.cc
      ../../../source/daemons/qmaster/sge_reporting_qmaster.cc
      ../../../source/daemons/qmaster/sge_rusage.cc
      ../../../source/daemons/qmaster/sge_qinstance_qmaster.cc
      ../../../source/daemons/qmaster/sge_qmod_qmaster.cc
      ../../../source/daemons/qmaster/sge_job_qmaster.cc
      ../../../source/daemons/qmaster/sge_give_jobs.cc
      ../../../source/daemons/qmaster/sge_subordinate_qmaster.cc
      ../../../source/daemons/qmaster/reschedule.cc
      ../../../source/daemons/qmaster/sge_persistence_qmaster.cc
      ../../../source/daemons/qmaster/job_exit.cc
      ../../../source/daemons/qmaster/sge_task_depend.cc
      ../../../source/daemons/qmaster/sge_host_qmaster.cc
      ../../../source/daemons/qmaster/sge_job_enforce_limit.cc
      ../../../source/daemons/qmaster/qmaster_to_execd.cc
      ../../../source/daemons/qmaster/sge_cqueue_qmaster.cc
      ../../../source/daemons/qmaster/sge_userprj_qmaster.cc
      ../../../source/daemons/qmaster/configuration_qmaster.cc
      ../../../source/daemons/qmaster/sge_userset_qmaster.cc
      ../../../source/daemons/qmaster/sge_resource_quota_qmaster.cc
      ../../../source/daemons/qmaster/sge_sharetree_qmaster.cc
      ../../../source/daemons/qmaster/sge_c_gdi.cc
      ../../../source/daemons/qmaster/sge_pe_qmaster.cc
      ../../../source/daemons/qmaster/sge_ckpt_qmaster.cc
      ../../../source/daemons/qmaster/sge_hgroup_qmaster.cc
      ../../../source/daemons/qmaster/sge_thread_scheduler.cc
      ../../../source/daemons/qmaster/sge_qmaster_threads.cc
      ../../../source/daemons/qmaster/sge_sched_job_category.cc
      ../../../source/daemons/qmaster/sge_sched_process_events.cc
      ../../../source/daemons/qmaster/sge_sched_prepare_data.cc
      ../../../source/daemons/qmaster/sge_sched_thread.cc
      ../../../source/daemons/qmaster/sge_sched_thread_rsmap.cc
      ../../../source/daemons/qmaster/sge_sched_order.cc
      ../../../source/daemons/qmaster/sge_thread_main.cc
      ../../../source/daemons/qmaster/sched_conf_qmaster.cc
      ../../../source/daemons/qmaster/sge_centry_qmaster.cc
      ../../../source/daemons/qmaster/sge_follow.cc
      ../../../source/daemons/qmaster/sge_job_verify.cc
      ../../../source/daemons/qmaster/setup_qmaster.cc
      ../../../source/daemons/qmaster/sge_manop_qmaster.cc
      ../../../source/daemons/qmaster/sge_thread_signaler.cc
      ../../../source/daemons/qmaster/sge_thread_utility.cc
      ../../../source/common/execution_states.cc
      ../../../source/common/sge_options.cc
      ../../../source/common/sig_handlers.cc
      ../../../source/common/usage.cc)
target_include_directories(test_qmaster_calendar PRIVATE "./")
target_link_libraries(
        test_qmaster_calendar
        PRIVATE daemonscommon ${SPOOLING_LIBS} sched mir evc evm sgeobj gdi cull comm commlists uti
        ${SGE_LIBS} ${GPERFTOOLS_PROFILER})
add_test(NAME test_qmaster_calendar COMMAND test_qmaster_calendar)

if (INSTALL_SGE_TEST)
   install(TARGETS test_qmaster_timed_event DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_qmaster_calendar DESTINATION testbin/${SGE_ARCH})
endif ()

