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

# test/libs/cull

add_executable(test_cull_hash test_cull_hash.cc)
target_include_directories(test_cull_hash PRIVATE "./")
target_link_libraries(test_cull_hash PRIVATE cull uti commlists ${SGE_LIBS})
add_test(NAME test_cull_hash COMMAND test_cull_hash)

add_executable(test_cull_list test_cull_list.cc)
target_include_directories(test_cull_list PRIVATE "./")
target_link_libraries(test_cull_list PRIVATE cull uti commlists ${SGE_LIBS})
add_test(NAME test_cull_list COMMAND test_cull_list)

add_executable(test_cull_observe test_cull_observe.cc)
target_include_directories(test_cull_observe PRIVATE "./")
target_link_libraries(test_cull_observe PRIVATE cull uti commlists ${SGE_LIBS})
add_test(NAME test_cull_observe COMMAND test_cull_observe)

add_executable(test_cull_pack test_cull_pack.cc)
target_include_directories(test_cull_pack PRIVATE "./")
target_link_libraries(test_cull_pack PRIVATE cull uti commlists ${SGE_LIBS})
add_test(NAME test_cull_pack COMMAND test_cull_pack)

add_executable(test_cull_enumeration test_cull_enumeration.cc)
target_include_directories(test_cull_enumeration PRIVATE "./")
target_link_libraries(test_cull_enumeration PRIVATE cull uti commlists ${SGE_LIBS})
add_test(NAME test_cull_enumeration COMMAND test_cull_enumeration)

if (INSTALL_SGE_TEST)
   install(TARGETS test_cull_hash DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_list DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_observe DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_pack DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_enumeration DESTINATION testbin/${SGE_ARCH})
endif ()
