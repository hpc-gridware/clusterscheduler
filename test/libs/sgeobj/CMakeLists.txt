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

# test/libs/sgeobj

add_executable(test_sgeobj_eval_expression test_sgeobj_eval_expression.cc)
target_include_directories(test_sgeobj_eval_expression PRIVATE "./")
target_link_libraries(test_sgeobj_eval_expression PRIVATE sgeobj gdi cull commlists uti ${SGE_LIBS})
add_test(NAME test_sgeobj_eval_expression COMMAND test_sgeobj_eval_expression)

add_executable(test_sgeobj_performance test_sgeobj_performance.cc)
target_include_directories(test_sgeobj_performance PRIVATE "./")
target_link_libraries(test_sgeobj_performance PRIVATE sgeobj gdi cull commlists uti ${SGE_LIBS})
add_test(NAME test_sgeobj_performance COMMAND test_sgeobj_performance)

add_executable(test_sgeobj_attr test_sgeobj_attr.cc)
target_include_directories(test_sgeobj_attr PRIVATE "./")
target_link_libraries(test_sgeobj_attr PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_sgeobj_attr COMMAND test_sgeobj_attr)

add_executable(test_sgeobj_fgl test_sgeobj_fgl.cc)
target_include_directories(test_sgeobj_fgl PRIVATE "./")
target_link_libraries(test_sgeobj_fgl PRIVATE sgeobj cull commlists uti ${SGE_LIBS})
add_test(NAME test_sgeobj_fgl COMMAND test_sgeobj_fgl)

add_executable(test_sgeobj_HostTopology test_sgeobj_HostTopology.cc)
target_include_directories(test_sgeobj_HostTopology PRIVATE "./")
target_link_libraries(test_sgeobj_HostTopology PRIVATE sched gdi sgeobj gdi cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_sgeobj_HostTopology COMMAND test_sgeobj_HostTopology)

add_executable(test_sgeobj_object test_sgeobj_object.cc)
target_include_directories(test_sgeobj_object PRIVATE "./")
target_link_libraries(test_sgeobj_object PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_sgeobj_object COMMAND test_sgeobj_object)

add_executable(test_sgeobj_range test_sgeobj_range.cc)
target_include_directories(test_sgeobj_range PRIVATE "./")
target_link_libraries(test_sgeobj_range PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_sgeobj_range COMMAND test_sgeobj_range)

add_executable(test_sgeobj_resource_quota test_sgeobj_resource_quota.cc)
target_include_directories(test_sgeobj_resource_quota PRIVATE "./")
target_link_libraries(test_sgeobj_resource_quota PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_sgeobj_resource_quota COMMAND test_sgeobj_resource_quota)

add_executable(test_sgeobj_schedd_conf test_sgeobj_schedd_conf.cc)
target_include_directories(test_sgeobj_schedd_conf PRIVATE "./")
target_link_libraries(test_sgeobj_schedd_conf PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_sgeobj_schedd_conf COMMAND test_sgeobj_schedd_conf)

add_executable(test_sgeobj_Session test_sgeobj_Session.cc)
target_include_directories(test_sgeobj_Session PRIVATE "./")
target_link_libraries(test_sgeobj_Session PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_sgeobj_Session COMMAND test_sgeobj_Session)

add_executable(test_sgeobj_utility test_sgeobj_utility.cc)
target_include_directories(test_sgeobj_utility PRIVATE "./")
target_link_libraries(test_sgeobj_utility PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_sgeobj_utility COMMAND test_sgeobj_utility)

if (INSTALL_SGE_TEST)
   install(TARGETS test_sgeobj_eval_expression DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_sgeobj_performance DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_sgeobj_attr DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_sgeobj_fgl DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_sgeobj_HostTopology DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_sgeobj_object DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_sgeobj_range DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_sgeobj_resource_quota DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_sgeobj_schedd_conf DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_sgeobj_Session DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_sgeobj_utility DESTINATION testbin/${SGE_ARCH})
endif ()
