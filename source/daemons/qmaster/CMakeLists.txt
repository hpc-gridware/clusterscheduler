#___INFO__MARK_BEGIN_NEW__
###########################################################################
#  
#  Copyright 2023-2024 HPC-Gridware GmbH
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

# source/daemons/qmaster

set(QMASTER_SOURCES
        configuration_qmaster.cc
        job_exit.cc
        job_report_qmaster.cc
        ocs_BaseAccountingFileWriter.cc
        ocs_BaseReportingFileWriter.cc
        ocs_CategoryQmaster.cc
        ocs_JsonAccountingFileWriter.cc
        ocs_JsonReportingFileWriter.cc
        ocs_MirrorReaderDataStore.cc
        ocs_MirrorServerDataStore.cc
        ocs_MirrorListenerDataStore.cc
        ocs_MonitoringFileWriter.cc
        ocs_ReportingFileWriter.cc
        ocs_thread_mirror.cc
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
        sge_thread_utility.cc
        sge_thread_reader.cc
        sge_userprj_qmaster.cc
        sge_userset_qmaster.cc
        sge_utility_qmaster.cc
        ../../../../${PROJECT_FEATURES}/source/daemons/qmaster/TestClass.cc
        ../../common/sig_handlers.cc
        ../../common/sge_options.cc
        ../../common/usage.cc
        ../../common/execution_states.cc
)

# normal qmaster without python

add_executable(
        sge_qmaster
        ${QMASTER_SOURCES}
)

target_include_directories(sge_qmaster PUBLIC "./" ${SGE_INCLUDES})

target_link_libraries(
         sge_qmaster
         PUBLIC daemonscommon sched evm mir evc gdi sgeobj cull comm commlists uti
         PRIVATE ${SGE_LIBS} ${SPOOLING_LIBS} ${GPERFTOOLS_PROFILER}
)

if (INSTALL_SGE_BIN)
   install(TARGETS sge_qmaster
           DESTINATION bin/${SGE_ARCH})
endif ()


# qmaster with python

if (WITH_PYTHON)
   add_executable(
           sge_qmaster.python
           ${QMASTER_SOURCES}
   )

   target_compile_definitions(sge_qmaster.python PRIVATE WITH_PYTHON_IN_QMASTER)

   target_include_directories(sge_qmaster.python
           PUBLIC "./" ${SGE_INCLUDES}
           PRIVATE ${PYTHON_ALL_INCLUDE_DIRS}
   )

   target_link_libraries(
           sge_qmaster.python
           PUBLIC daemonscommon sched evm mir evc gdi sgeobj cull comm commlists uti
           PRIVATE ${SGE_LIBS} ${SPOOLING_LIBS} ${GPERFTOOLS_PROFILER} ${PYTHON_ALL_LIBRARIES}
   )

   if (INSTALL_SGE_BIN)
      install(TARGETS sge_qmaster.python
              DESTINATION bin/${SGE_ARCH})
   endif ()
endif()

