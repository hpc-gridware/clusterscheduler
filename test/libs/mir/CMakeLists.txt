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

# test/libs/mir

add_executable(test_mir_basic test_mir_basic.cc  ../../../source/common/sig_handlers.cc)
target_include_directories(test_mir_basic PRIVATE "./")
target_link_libraries(test_mir_basic PRIVATE mir evc gdi sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_mir_basic COMMAND test_mir_basic)

if (INSTALL_SGE_TEST)
   install(TARGETS test_mir_basic DESTINATION testbin/${SGE_ARCH})
endif ()
