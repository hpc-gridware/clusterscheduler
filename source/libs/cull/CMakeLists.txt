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

# source/libs/cull
set(LIBRARY_NAME cull)
set(LIBRARY_SOURCES
        cull_db.cc
        cull_dump_scan.cc
        cull_file.cc
        cull_hash.cc
        cull_lerrno.cc
        cull_list.cc
        cull_multitype.cc
        cull_observe.cc
        cull_pack.cc
        cull_parse.cc
        cull_sort.cc
        cull_state.cc
        cull_tree.cc
        cull_what.cc
        cull_what_print.cc
        cull_where.cc
        pack.cc)
set(LIBRARY_INCLUDES "./")

if (SGE_ARCH MATCHES "lx-arm.*")
   # Raspians gcc 10.2 compiler shows incorrect warnings for memcpy calls
   set_source_files_properties(pack.cc PROPERTIES COMPILE_FLAGS "-Wno-array-bounds -Wno-stringop-overflow")
endif()

# on Ubuntu 24.04 the XDR related system header causes warnings
# set_source_files_properties(pack.cc PROPERTIES COMPILE_FLAGS "-Wno-changes-meaning -Wno-pedantic")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES} ${LIBRARY_INCLUDES})
