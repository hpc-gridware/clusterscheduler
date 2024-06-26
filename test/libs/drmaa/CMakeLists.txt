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

# test/libs/drmaa

add_executable(test_drmaa_issue1768 ../../../test/libs/drmaa/test_drmaa_issue1768.cc)
target_include_directories(test_drmaa_issue1768 PRIVATE "./")
target_link_libraries(test_drmaa_issue1768 PRIVATE drmaa ${SGE_LIBS})
add_test(NAME test_drmaa_issue1768 COMMAND test_drmaa_issue1768)

add_executable(test_drmaa_issue1832 ../../../test/libs/drmaa/test_drmaa_issue1832.cc)
target_include_directories(test_drmaa_issue1832 PRIVATE "./")
target_link_libraries(test_drmaa_issue1832 PRIVATE drmaa ${SGE_LIBS})
add_test(NAME test_drmaa_issue1832 COMMAND test_drmaa_issue1832)

add_executable(test_drmaa_issue1892 ../../../test/libs/drmaa/test_drmaa_issue1892.cc)
target_include_directories(test_drmaa_issue1892 PRIVATE "./")
target_link_libraries(test_drmaa_issue1892 PRIVATE drmaa ${SGE_LIBS})
add_test(NAME test_drmaa_issue1892 COMMAND test_drmaa_issue1892)

add_executable(test_drmaa_mcpu ../../../test/libs/drmaa/test_drmaa_mcpu.cc)
target_include_directories(test_drmaa_mcpu PRIVATE "./")
target_link_libraries(test_drmaa_mcpu PRIVATE drmaa ${SGE_LIBS})
add_test(NAME test_drmaa_mcpu COMMAND test_drmaa_mcpu)

add_executable(test_drmaa_no_bin ../../../test/libs/drmaa/test_drmaa_no_bin.cc)
target_include_directories(test_drmaa_no_bin PRIVATE "./")
target_link_libraries(test_drmaa_no_bin PRIVATE drmaa ${SGE_LIBS})
add_test(NAME test_drmaa_no_bin COMMAND test_drmaa_no_bin)

add_executable(test_drmaa_perf ../../../test/libs/drmaa/test_drmaa_perf.cc)
target_include_directories(test_drmaa_perf PRIVATE "./")
target_link_libraries(test_drmaa_perf PRIVATE drmaa ${SGE_LIBS})
add_test(NAME test_drmaa_perf COMMAND test_drmaa_perf)

add_executable(test_drmaa_sync ../../../test/libs/drmaa/test_drmaa_sync.cc)
target_include_directories(test_drmaa_sync PRIVATE "./")
target_link_libraries(test_drmaa_sync PRIVATE drmaa ${SGE_LIBS})
add_test(NAME test_drmaa_sync COMMAND test_drmaa_sync)

if (INSTALL_SGE_TEST)
   install(TARGETS test_drmaa DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_drmaa_issue1768 DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_drmaa_issue1832 DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_drmaa_issue1892 DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_drmaa_mcpu DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_drmaa_no_bin DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_drmaa_perf DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_drmaa_sync DESTINATION testbin/${SGE_ARCH})
endif ()
