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

# test_spool_performance and test_spool_mt are correct but long-running, so they
# are kept out of the default ctest run. Opt in by reconfiguring with
# -DENABLE_LONG_TESTS=ON (same switch as the other long-running unit tests).
if (NOT ENABLE_LONG_TESTS)
   set_tests_properties(test_spool_performance test_spool_mt PROPERTIES DISABLED TRUE)
endif ()

add_executable(test_spool_utilities test_spool_utilities.cc)
target_include_directories(test_spool_utilities PRIVATE "./")
target_link_libraries(test_spool_utilities PRIVATE ${SPOOLING_LIBS} gdi sched sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_spool_utilities COMMAND test_spool_utilities)

add_executable(test_spool_duplicates test_spool_duplicates.cc)
target_include_directories(test_spool_duplicates PRIVATE "./")
target_link_libraries(test_spool_duplicates PRIVATE spool sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_spool_duplicates COMMAND test_spool_duplicates)

# CS-3120: a dot file left behind by a failed rename must not cost a job its tasks
add_executable(test_spool_classic_job test_spool_classic_job.cc)
target_include_directories(test_spool_classic_job PRIVATE "./")
target_link_libraries(test_spool_classic_job PRIVATE spoolc_static spool gdi sched sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_spool_classic_job COMMAND test_spool_classic_job)

# Golden output of the flatfile/JSON spooling of every spooled object type, for proving a
# change of the spooling field tables byte identical: run "test_spool_golden <dir>" before
# and after the change and compare with "diff -r". Without a directory (as ctest runs it)
# it only checks that every text reads back and writes again unchanged.
add_executable(test_spool_golden test_spool_golden.cc)
target_include_directories(test_spool_golden PRIVATE "./")
target_link_libraries(test_spool_golden PRIVATE spoolc_static spool gdi sched sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_spool_golden COMMAND test_spool_golden)

# Round trip of objects through the Berkeley DB spooling backend in a temporary directory: written bytes,
# record keys, read-back content and ownership of the objects written. Built only where the backend is.
if (${WITH_SPOOL_BERKELEYDB})
   add_executable(test_spool_bdb_roundtrip test_spool_bdb_roundtrip.cc)
   target_include_directories(test_spool_bdb_roundtrip PRIVATE "./")
   target_link_libraries(test_spool_bdb_roundtrip PRIVATE spoolb_static spool gdi sched sgeobj cull comm commlists
         uti berkeleydb ${SGE_LIBS})
   add_test(NAME test_spool_bdb_roundtrip COMMAND test_spool_bdb_roundtrip)
   if (INSTALL_SGE_TEST)
      install(TARGETS test_spool_bdb_roundtrip DESTINATION testbin/${SGE_ARCH})
   endif ()
endif ()

add_executable(test_spool test_spool.cc ../../../source/common/sig_handlers.cc)
target_include_directories(test_spool PRIVATE "./")
target_link_libraries(test_spool PRIVATE mir evc ${SPOOLING_LIBS} gdi sched sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_spool COMMAND test_spool)

# test_spool requires arguments (<method> <shared lib> <arguments>) and cannot
# be run by a bare ctest, so it is always disabled (kept registered only for
# discoverability).
set_tests_properties(test_spool PROPERTIES DISABLED TRUE)

if (INSTALL_SGE_TEST)
   install(TARGETS test_spool DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_spool_mt DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_spool_performance DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_spool_utilities DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_spool_duplicates DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_spool_classic_job DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_spool_golden DESTINATION testbin/${SGE_ARCH})
endif ()
