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

# source/libs/sched
set(LIBRARY_NAME sched)
set(LIBRARY_SOURCES
      debit.cc
      load_correction.cc
      ocs_CategorySchedd.cc
      schedd_message.cc
      schedd_monitor.cc
      sge_complex_schedd.cc
      sge_interactive_sched.cc
      sge_job_schedd.cc
      sge_orders.cc
      sge_pe_schedd.cc
      sge_qeti.cc
      sge_resource_utilization.cc
      sge_schedd_text.cc
      sge_select_queue.cc
      sge_select_queue_rqs.cc
      sge_serf.cc
      sge_sharetree_printing.cc
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


