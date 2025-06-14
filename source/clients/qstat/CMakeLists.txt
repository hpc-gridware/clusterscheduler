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

# source/clients/qstat
add_executable(
      qstat
        ocs_qstat.cc
        ocs_qstat_cmdline.cc
        ocs_qstat_filter.cc
        ocs_qstat_xml.cc
      ../../common/parse_job_cull.cc
      ../../common/parse_qsub.cc
      ../../common/sig_handlers.cc
      ../../common/sge_options.cc
      ../../common/usage.cc)

target_include_directories(qstat PRIVATE ${SGE_INCLUDES} "../../libs/uti"
      "../../libs/cull" "../../libs/sgeobj")

target_link_libraries(
      qstat
      PRIVATE clientscommon
      sched
      gdi
      sgeobj
      cull
      comm
      commlists
      uti
      ${SGE_LIBS})

if (INSTALL_SGE_BIN_CLIENT)
   install(TARGETS qstat DESTINATION bin/${SGE_ARCH})

   install(
         CODE "execute_process(
    COMMAND /bin/sh -c \"
    cd ${CMAKE_INSTALL_PREFIX}/bin/${SGE_ARCH}
    ln -s qstat qselect
    \"
)")
endif ()
