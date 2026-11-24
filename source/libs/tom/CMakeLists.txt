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

# source/libs/tom: the typed object model (CS-3043), the list library of the product (CS-3103)
#
# The storage layer (the structures, the descriptor layout, the hash tables) lives in detail/;
# the old list library and its interface are gone.
add_library(tom STATIC)
# the list library calls the utility library, so it is linked after it (CS-3088)
target_link_libraries(tom PUBLIC uti)
target_include_directories(tom PUBLIC ${SGE_INCLUDES})

target_sources(tom PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/detail/ocs_Hash.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/ocs_Descr.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/ocs_DescrRegistryProbe.cc
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

if (ENABLE_TOM_STATS)
   target_sources(tom PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/detail/ocs_Stats.cc)
endif ()

if (SGE_ARCH MATCHES "lx-arm.*")
   # Raspians gcc 10.2 compiler shows incorrect warnings for memcpy calls (formerly set for the old pack.cc)
   set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/ocs_PackCore.cc PROPERTIES COMPILE_FLAGS
                               "-Wno-array-bounds -Wno-stringop-overflow")
endif ()
