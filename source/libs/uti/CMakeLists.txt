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

# source/libs/uti
set(LIBRARY_NAME uti)
set(LIBRARY_SOURCES
      config_file.cc
      ocs_JsonUtil.cc
      ocs_cond.cc
      sge_afsutil.cc
      sge_arch.cc
      sge_binding_hlp.cc
      sge_binding_parse.cc
      sge_bitfield.cc
      sge_bootstrap.cc
      sge_bootstrap_env.cc
      sge_bootstrap_files.cc
      sge_component.cc
      sge_csp_path.cc
      sge_dstring.cc
      sge_edit.cc
      sge_err.cc
      sge_error_class.cc
      sge_fgl.cc
      sge_getloadavg.cc
      ocs_gperf.cc
      sge_hostname.cc
      sge_htable.cc
      sge_io.cc
      sge_language.cc
      sge_loadmem.cc
      sge_lock.cc
      sge_lock_fifo.cc
      sge_log.cc
      sge_monitor.cc
      sge_mtutil.cc
      sge_nprocs.cc
      sge_os.cc
      sge_parse_args.cc
      sge_parse_num_par.cc
      sge_profiling.cc
      sge_pty.cc
      sge_rmon_macros.cc
      sge_rmon_monitoring_level.cc
      sge_signal.cc
      sge_sl.cc
      sge_smf.cc
      sge_spool.cc
      sge_stdio.cc
      sge_stdlib.cc
      sge_string.cc
      sge_thread_ctrl.cc
      sge_time.cc
      sge_tmpnam.cc
      ocs_topology.cc
      sge_tq.cc
      sge_uidgid.cc
      sge_unistd.cc)
set(LIBRARY_INCLUDES "./" ${SGE_INCLUDES})

# CS-199 makes use of readdir_r/readdir64_r which is deprecated for Linux only
set_source_files_properties(sge_unistd.cc PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${LIBRARY_INCLUDES})

