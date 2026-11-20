#___INFO__MARK_BEGIN_NEW__
###########################################################################
#
#  Copyright 2026 HPC-Gridware GmbH
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

# source/libs/tom: the typed object model (CS-3043)
#
# The typed core and the list library are one implementation with two
# interfaces, so the sources here are built into the cull library rather than
# into a library of their own.
# The storage layer moved here from the list library (CS-3088): the structures,
# the descriptor layout, the hash tables and the error number. They keep their names until the list library is removed.
target_sources(cull PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/cull_hash.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/cull_lerrno.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/cull_state.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/ocs_cull_descr.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/ocs_DumpCore.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/ocs_ElementCore.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/ocs_FieldException.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/ocs_FreeAccess.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/ocs_Host.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/ocs_ListCore.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/ocs_ListOpsCore.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/ocs_Model.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/ocs_PrintCore.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/ocs_Raw.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/ocs_SelectCore.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/ocs_SoftPath.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/ocs_PackCore.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/ocs_SortCore.cc
)

if (ENABLE_CULL_STATS)
   target_sources(cull PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cull_stats.cc)
endif ()
