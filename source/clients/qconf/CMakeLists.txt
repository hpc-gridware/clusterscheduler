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

# source/clients/qconf
add_executable(
      qconf
      oge_qconf_cqueue.cc
      oge_qconf_centry.cc
      sge_qconf_hgroup.cc
      oge_qconf_rqs.cc
      oge_qconf.cc
      oge_qconf_parse.cc
      oge_qconf_acl.cc
      ../../common/sig_handlers.cc
      ../../common/sge_options.cc
      ../../common/usage.cc)
target_include_directories(qconf PUBLIC ${SGE_INCLUDES})
target_link_libraries(
      qconf
      PUBLIC clientscommon
      spoolc_static
      spool
      sched
      comm
      commlists
      gdi
      sgeobj
      cull
      uti
      ${SGE_LIBS})

if (INSTALL_SGE_BIN)
   install(TARGETS qconf DESTINATION bin/${SGE_ARCH})
endif ()
