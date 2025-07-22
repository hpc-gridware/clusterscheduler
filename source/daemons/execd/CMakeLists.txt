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

# source/daemons/execd
add_executable(
      sge_execd
      execd.cc
      dispatcher.cc
      exec_job.cc
      execd_ck_to_do.cc
      execd_get_new_conf.cc
      execd_job_exec.cc
      execd_kill_execd.cc
      execd_profiling.cc
      execd_signal_queue.cc
        ocs_execd_systemd.cc
      execd_ticket.cc
      get_path.cc
      job_report_execd.cc
      load_avg.cc
      ptf.cc
      reaper_execd.cc
      setup_execd.cc
      sge_load_sensor.cc
      sge_report_execd.cc
      tmpdir.cc
      ../../common/sig_handlers.cc
      ../../common/sge_options.cc
      ../../common/usage.cc
      ../../common/execution_states.cc)

target_include_directories(sge_execd PRIVATE ${SGE_INCLUDES})

target_link_libraries(
      sge_execd
      PRIVATE daemonscommon
      spoolc_static
      spool
      sched
      gdi
      sgeobj
      cull
      comm
      commlists
      uti
      ${SGE_TOPO_LIB}
      ${SGE_LIBS})

if (INSTALL_SGE_BIN_EXEC)
   install(TARGETS sge_execd DESTINATION bin/${SGE_ARCH})
endif ()
