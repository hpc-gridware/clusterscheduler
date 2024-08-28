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

# clusterscheduler/doc/markdown/man/man5

# build all man pages from section 5
set(PAGES sge_access_list sge_accounting sge_aliases sge_bootstrap sge_calendar_conf sge_checkpoint sge_complex
      sge_conf sge_host_aliases sge_host_conf sge_hostgroup sge_pe sge_priority sge_project sge_qstat sge_qtask
      sge_queue_conf sge_reporting sge_request sge_resource_quota sge_sched_conf sge_share_tree sge_user)
build_markdown_man("5" PAGES "0")

# target for building all troff man pages from section 5
# @todo is there a better way of listing them? Didn't find an easily comprehensible way to use foreach().
add_custom_target(troffman5 ALL DEPENDS
      ${CMAKE_CURRENT_BINARY_DIR}/sge_access_list.5
      ${CMAKE_CURRENT_BINARY_DIR}/sge_accounting.5
      ${CMAKE_CURRENT_BINARY_DIR}/sge_aliases.5
      ${CMAKE_CURRENT_BINARY_DIR}/sge_bootstrap.5
      ${CMAKE_CURRENT_BINARY_DIR}/sge_calendar_conf.5
      ${CMAKE_CURRENT_BINARY_DIR}/sge_checkpoint.5
      ${CMAKE_CURRENT_BINARY_DIR}/sge_complex.5
      ${CMAKE_CURRENT_BINARY_DIR}/sge_conf.5
      ${CMAKE_CURRENT_BINARY_DIR}/sge_host_aliases.5
      ${CMAKE_CURRENT_BINARY_DIR}/sge_host_conf.5
      ${CMAKE_CURRENT_BINARY_DIR}/sge_hostgroup.5
      ${CMAKE_CURRENT_BINARY_DIR}/sge_pe.5
      ${CMAKE_CURRENT_BINARY_DIR}/sge_priority.5
      ${CMAKE_CURRENT_BINARY_DIR}/sge_project.5
      ${CMAKE_CURRENT_BINARY_DIR}/sge_qstat.5
      ${CMAKE_CURRENT_BINARY_DIR}/sge_qtask.5
      ${CMAKE_CURRENT_BINARY_DIR}/sge_queue_conf.5
      ${CMAKE_CURRENT_BINARY_DIR}/sge_reporting.5
      ${CMAKE_CURRENT_BINARY_DIR}/sge_request.5
      ${CMAKE_CURRENT_BINARY_DIR}/sge_resource_quota.5
      ${CMAKE_CURRENT_BINARY_DIR}/sge_sched_conf.5
      ${CMAKE_CURRENT_BINARY_DIR}/sge_share_tree.5
      ${CMAKE_CURRENT_BINARY_DIR}/sge_user.5
)

# installation
if (INSTALL_SGE_DOC)
   foreach (PAGE ${PAGES})
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PAGE}.5 DESTINATION man/man5)
   endforeach ()
endif ()
