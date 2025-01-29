#___INFO__MARK_BEGIN_NEW__
###########################################################################
#  
#  Copyright 2024 HPC-Gridware GmbH
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

# test/daemons/common

add_executable(test_common_category test_common_category.cc)
target_include_directories(test_common_category PRIVATE "./")
target_link_libraries(test_common_category PRIVATE daemonscommon sched sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_common_category COMMAND test_common_category)

add_executable(test_common_heartbeat test_common_heartbeat.cc)
target_include_directories(test_common_heartbeat PRIVATE "./")
target_link_libraries(test_common_heartbeat PRIVATE daemonscommon commlists uti ${SGE_LIBS})
add_test(NAME test_common_heartbeat COMMAND test_common_heartbeat)

if (INSTALL_SGE_TEST)
   install(TARGETS test_common_category DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_common_heartbeat DESTINATION testbin/${SGE_ARCH})
endif ()
