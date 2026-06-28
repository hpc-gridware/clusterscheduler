#___INFO__MARK_BEGIN_NEW__
###########################################################################
#  
#  Copyright 2024,2026 HPC-Gridware GmbH
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
#
# These are DRMAA *integration* tests: each connects to a running qmaster via
# drmaa_init() and submits a real job, so they only pass against a live, fully
# installed cluster (SGE_ROOT/CELL set, exec hosts up). Most require the path to
# a job binary as argv[1]. The arguments below mirror what the Tcl testsuite
# passes via start_test_bin (see checktree/bugs/issuezilla/{1768,1832,1892,1270}
# and checktree/bugs/issuezilla/1529); without them the tests fail immediately
# with 'job template must have "drmaa_remote_command" attribute set'.
#
# Several need the cluster's example sleeper script (it must be a real *script* -
# test_drmaa_issue1832 even rejects any path not containing "sleeper.sh", and
# test_drmaa_no_bin submits with "-b n"). We resolve it via $ENV{SGE_ROOT},
# which is evaluated at *configure* time, so SGE_ROOT must be set when cmake runs.
#
# Caveats that args alone cannot satisfy (these may still fail under bare ctest):
#  - test_drmaa_mcpu  : takes no argv; hardcodes CMD "/tmp/sleeper.sh" and cell
#                       "default", and has no Tcl driver - left unparameterized.
#  - test_drmaa_issue1768 : the Tcl test first defines the "issue1768" qtask
#                       category (append_to_qtask_file); an undefined category is
#                       merely ignored, so it should still submit successfully.
#  - test_drmaa_sync  : sleeps 60s unconditionally and returns its internal
#                       counter as the exit code, so runtime is ~60s by design.
#
# NOTE: changing add_test() only takes effect after re-running cmake (it
# regenerates CTestTestfile.cmake); a bare "ctest" reuses the old definitions.

add_executable(test_drmaa_issue1768 ../../../test/libs/drmaa/test_drmaa_issue1768.cc)
target_include_directories(test_drmaa_issue1768 PRIVATE "./")
target_link_libraries(test_drmaa_issue1768 PRIVATE drmaa ${SGE_LIBS})
add_test(NAME test_drmaa_issue1768 COMMAND test_drmaa_issue1768 /bin/date issue1768)

add_executable(test_drmaa_issue1832 ../../../test/libs/drmaa/test_drmaa_issue1832.cc)
target_include_directories(test_drmaa_issue1832 PRIVATE "./")
target_link_libraries(test_drmaa_issue1832 PRIVATE drmaa ${SGE_LIBS})
add_test(NAME test_drmaa_issue1832 COMMAND test_drmaa_issue1832 $ENV{SGE_ROOT}/examples/jobs/sleeper.sh)

add_executable(test_drmaa_issue1892 ../../../test/libs/drmaa/test_drmaa_issue1892.cc)
target_include_directories(test_drmaa_issue1892 PRIVATE "./")
target_link_libraries(test_drmaa_issue1892 PRIVATE drmaa ${SGE_LIBS})
add_test(NAME test_drmaa_issue1892 COMMAND test_drmaa_issue1892 /bin/date)

add_executable(test_drmaa_mcpu ../../../test/libs/drmaa/test_drmaa_mcpu.cc)
target_include_directories(test_drmaa_mcpu PRIVATE "./")
target_link_libraries(test_drmaa_mcpu PRIVATE drmaa ${SGE_LIBS})
add_test(NAME test_drmaa_mcpu COMMAND test_drmaa_mcpu)

add_executable(test_drmaa_no_bin ../../../test/libs/drmaa/test_drmaa_no_bin.cc)
target_include_directories(test_drmaa_no_bin PRIVATE "./")
target_link_libraries(test_drmaa_no_bin PRIVATE drmaa ${SGE_LIBS})
add_test(NAME test_drmaa_no_bin COMMAND test_drmaa_no_bin $ENV{SGE_ROOT}/examples/jobs/sleeper.sh)

add_executable(test_drmaa_perf ../../../test/libs/drmaa/test_drmaa_perf.cc)
target_include_directories(test_drmaa_perf PRIVATE "./")
target_link_libraries(test_drmaa_perf PRIVATE drmaa ${SGE_LIBS})
add_test(NAME test_drmaa_perf COMMAND test_drmaa_perf -jobs 1 /bin/date)

add_executable(test_drmaa_sync ../../../test/libs/drmaa/test_drmaa_sync.cc)
target_include_directories(test_drmaa_sync PRIVATE "./")
target_link_libraries(test_drmaa_sync PRIVATE drmaa ${SGE_LIBS})
add_test(NAME test_drmaa_sync COMMAND test_drmaa_sync $ENV{SGE_ROOT}/examples/jobs/sleeper.sh)

# All of the DRMAA tests above are cluster/integration tests: they need a live
# qmaster, exec hosts and TS-style setup (job categories, the example sleeper
# script), so they are kept out of the default ctest run. Opt in by reconfiguring
# with -DENABLE_DRMAA_TESTS=ON; once enabled, select them with: ctest -R test_drmaa
# (ENABLE_DRMAA_TESTS is declared in the top-level CMakeLists.)
set(DRMAA_TESTS
   test_drmaa_issue1768
   test_drmaa_issue1832
   test_drmaa_issue1892
   test_drmaa_mcpu
   test_drmaa_no_bin
   test_drmaa_perf
   test_drmaa_sync
)
if (NOT ENABLE_DRMAA_TESTS)
   set_tests_properties(${DRMAA_TESTS} PROPERTIES DISABLED TRUE)
endif ()

# CS-2404 regression probe. Driven by the testsuite check at
# testsuite/src/checktree/system_tests/drmaa/precedence; not wired into
# CTest because it needs a live cluster + tmpdir/sge_request setup.
add_executable(test_drmaa_cs2404 ../../../test/libs/drmaa/test_drmaa_cs2404.cc)
target_include_directories(test_drmaa_cs2404 PRIVATE "./")
target_link_libraries(test_drmaa_cs2404 PRIVATE drmaa ${SGE_LIBS})

if (INSTALL_SGE_TEST)
   install(TARGETS test_drmaa DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_drmaa_issue1768 DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_drmaa_issue1832 DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_drmaa_issue1892 DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_drmaa_mcpu DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_drmaa_no_bin DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_drmaa_perf DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_drmaa_sync DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_drmaa_cs2404 DESTINATION testbin/${SGE_ARCH})
endif ()
