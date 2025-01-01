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

# source/libs/gdi
set(LIBRARY_NAME gdi)
set(LIBRARY_SOURCES
        ocs_GdiMode.cc
        ocs_GdiMulti.cc
        ocs_GdiTarget.cc
        ocs_GdiTask.cc
        ocs_gdi_execd_delivery.cc
      qm_name.cc
        ocs_gdi_client.cc
      sge_gdi.cc
      sge_gdi_data.cc
      sge_gdi_packet.cc
      sge_gdi_packet_internal.cc
      sge_gdi_packet_pb_cull.cc
      sge_qexec.cc
      sge_qtcsh.cc
      sge_security.cc)
set(LIBRARY_INCLUDES ${SGE_INCLUDES} "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${LIBRARY_INCLUDES})
