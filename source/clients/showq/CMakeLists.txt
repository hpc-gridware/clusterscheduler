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

# source/clients/showq
add_executable(
      showq
        ocs_showq.cc
        ocs_showq_cmdline_tacc.cc
        ocs_showq_support.cc
      ../../common/parse_job_cull.cc
      ../../common/parse_qsub.cc
      ../../common/sig_handlers.cc
)
target_include_directories(showq PUBLIC ${SGE_INCLUDES})
target_link_libraries(
      showq
      PUBLIC sched
      # evc
      gdi
      sgeobj
      cull
      comm
      commlists
      uti
      ${SGE_LIBS})

if (INSTALL_SGE_BIN)
   install(TARGETS showq DESTINATION examples/jobsbin/${SGE_ARCH})
endif ()
