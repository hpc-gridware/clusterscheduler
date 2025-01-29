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

# test/libs/spool

add_subdirectory(flatfile)

add_executable(test_spool_performance test_spool_performance.cc)
target_include_directories(test_spool_performance PRIVATE "./")
target_link_libraries(test_spool_performance PRIVATE ${SPOOLING_LIBS} gdi sched sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_spool_performance COMMAND test_spool_performance)

add_executable(test_spool_mt test_spool_mt.cc)
target_include_directories(test_spool_mt PRIVATE "./")
target_link_libraries(test_spool_mt PRIVATE ${SPOOLING_LIBS} gdi sched sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_spool_mt COMMAND test_spool_mt)

add_executable(test_spool_utilities test_spool_utilities.cc)
target_include_directories(test_spool_utilities PRIVATE "./")
target_link_libraries(test_spool_utilities PRIVATE ${SPOOLING_LIBS} gdi sched sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_spool_utilities COMMAND test_spool_utilities)

add_executable(test_spool test_spool.cc ../../../source/common/sig_handlers.cc)
target_include_directories(test_spool PRIVATE "./")
target_link_libraries(test_spool PRIVATE mir evc ${SPOOLING_LIBS} gdi sched sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_spool COMMAND test_spool)

if (INSTALL_SGE_TEST)
   install(TARGETS test_spool DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_spool_mt DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_spool_performance DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_spool_utilities DESTINATION testbin/${SGE_ARCH})
endif ()
