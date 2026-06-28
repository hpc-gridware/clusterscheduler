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

# test/libs/sched

add_executable(test_sched_eval_performance test_sched_eval_performance.cc)
target_include_directories(test_sched_eval_performance PRIVATE "./")
target_link_libraries(test_sched_eval_performance PRIVATE sched gdi sgeobj gdi cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_sched_eval_performance COMMAND test_sched_eval_performance)

# test_sched_eval_performance is correct but long-running, so it is kept out of
# the default ctest run. Opt in by reconfiguring with -DENABLE_LONG_TESTS=ON
# (same switch as the uti lock/deadlock, cull perf and sgeobj_fgl tests).
if (NOT ENABLE_LONG_TESTS)
   set_tests_properties(test_sched_eval_performance PROPERTIES DISABLED TRUE)
endif ()

add_executable(test_sched_resource_utilization test_sched_resource_utilization.cc)
target_include_directories(test_sched_resource_utilization PRIVATE "./")
target_link_libraries(test_sched_resource_utilization PRIVATE sched sgeobj gdi cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_sched_resource_utilization COMMAND test_sched_resource_utilization)

add_executable(test_sched_load_formula test_sched_load_formula.cc)
target_include_directories(test_sched_load_formula PRIVATE "./")
target_link_libraries(test_sched_load_formula PRIVATE sched sgeobj gdi cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_sched_load_formula COMMAND test_sched_load_formula)

if (INSTALL_SGE_TEST)
   install(TARGETS test_sched_eval_performance DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_sched_resource_utilization DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_sched_load_formula DESTINATION testbin/${SGE_ARCH})
endif ()
