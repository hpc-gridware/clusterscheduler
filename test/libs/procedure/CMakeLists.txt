#___INFO__MARK_BEGIN_NEW__
###########################################################################
#
#  Copyright 2024-2026 HPC-Gridware GmbH
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

# test/libs/procedure

add_executable(test_procedure_qquota_json test_procedure_qquota_json.cc)
target_include_directories(test_procedure_qquota_json PRIVATE "./")
target_link_libraries(test_procedure_qquota_json PRIVATE procedure sched gdi sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_procedure_qquota_json COMMAND test_procedure_qquota_json)

add_executable(test_procedure_qrstat_json test_procedure_qrstat_json.cc)
target_include_directories(test_procedure_qrstat_json PRIVATE "./")
target_link_libraries(test_procedure_qrstat_json PRIVATE procedure sched gdi sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_procedure_qrstat_json COMMAND test_procedure_qrstat_json)

add_executable(test_procedure_qquota_plain test_procedure_qquota_plain.cc)
target_include_directories(test_procedure_qquota_plain PRIVATE "./")
target_link_libraries(test_procedure_qquota_plain PRIVATE procedure sched gdi sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_procedure_qquota_plain COMMAND test_procedure_qquota_plain)

add_executable(test_procedure_qstat_summary test_procedure_qstat_summary.cc)
target_include_directories(test_procedure_qstat_summary PRIVATE "./")
target_link_libraries(test_procedure_qstat_summary PRIVATE procedure sched gdi sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_procedure_qstat_summary COMMAND test_procedure_qstat_summary)

if (INSTALL_SGE_TEST)
   install(TARGETS test_procedure_qquota_json DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_procedure_qrstat_json DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_procedure_qquota_plain DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_procedure_qstat_summary DESTINATION testbin/${SGE_ARCH})
endif ()

add_executable(test_procedure_reduction test_procedure_reduction.cc)
target_include_directories(test_procedure_reduction PRIVATE "./")
target_link_libraries(test_procedure_reduction PRIVATE procedure sched gdi sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_procedure_reduction COMMAND test_procedure_reduction)

add_executable(test_procedure_qconf_missing_argument test_procedure_qconf_missing_argument.cc)
target_include_directories(test_procedure_qconf_missing_argument PRIVATE "./")
# the qconf model reaches the flatfile spooling instructions and the client side
# of GDI, so it links what qconf links, comm after the libraries that use it
target_link_libraries(test_procedure_qconf_missing_argument PRIVATE procedure clientscommon spoolc_static spool sched
                      gdi sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_procedure_qconf_missing_argument COMMAND test_procedure_qconf_missing_argument)
if (INSTALL_SGE_TEST)
   install(TARGETS test_procedure_qconf_missing_argument DESTINATION testbin/${SGE_ARCH})
endif ()
