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

# test/libs/perf: performance of the list API, one source for CULL (9.1) and TOM (9.2)

add_executable(test_list_api_perf test_list_api_perf.cc)
target_include_directories(test_list_api_perf PRIVATE "./")
target_compile_definitions(test_list_api_perf PRIVATE OCS_PERF_API_TOM)
target_link_libraries(test_list_api_perf PRIVATE tom sgeobj uti commlists ${SGE_LIBS})
add_test(NAME test_list_api_perf COMMAND test_list_api_perf)

# the test measures for several minutes; it runs in ctest only with -DENABLE_LONG_TESTS=ON
if (NOT ENABLE_LONG_TESTS)
   set_tests_properties(test_list_api_perf PROPERTIES DISABLED TRUE)
endif ()

if (INSTALL_SGE_TEST)
   install(TARGETS test_list_api_perf DESTINATION testbin/${SGE_ARCH})
endif ()
