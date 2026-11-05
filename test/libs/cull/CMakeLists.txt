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

add_executable(test_cull_no_transfer test_cull_no_transfer.cc)
target_include_directories(test_cull_no_transfer PRIVATE "./")
target_link_libraries(test_cull_no_transfer PRIVATE cull uti commlists ${SGE_LIBS})
add_test(NAME test_cull_no_transfer COMMAND test_cull_no_transfer)

add_executable(test_cull_enumeration test_cull_enumeration.cc)
target_include_directories(test_cull_enumeration PRIVATE "./")
target_link_libraries(test_cull_enumeration PRIVATE cull uti commlists ${SGE_LIBS})
add_test(NAME test_cull_enumeration COMMAND test_cull_enumeration)

add_executable(test_cull_sort test_cull_sort.cc)
target_include_directories(test_cull_sort PRIVATE "./")
target_link_libraries(test_cull_sort PRIVATE cull uti commlists ${SGE_LIBS})
add_test(NAME test_cull_sort COMMAND test_cull_sort)

add_executable(test_cull_access test_cull_access.cc)
target_include_directories(test_cull_access PRIVATE "./")
target_link_libraries(test_cull_access PRIVATE cull uti commlists ${SGE_LIBS})
add_test(NAME test_cull_access COMMAND test_cull_access)

add_executable(test_cull_lookup test_cull_lookup.cc)
target_include_directories(test_cull_lookup PRIVATE "./")
target_link_libraries(test_cull_lookup PRIVATE cull uti commlists ${SGE_LIBS})
add_test(NAME test_cull_lookup COMMAND test_cull_lookup)

add_executable(test_cull_db test_cull_db.cc)
target_include_directories(test_cull_db PRIVATE "./")
target_link_libraries(test_cull_db PRIVATE cull uti commlists ${SGE_LIBS})
add_test(NAME test_cull_db COMMAND test_cull_db)

add_executable(test_cull_select_pack test_cull_select_pack.cc)
target_include_directories(test_cull_select_pack PRIVATE "./")
target_link_libraries(test_cull_select_pack PRIVATE cull uti commlists ${SGE_LIBS})
add_test(NAME test_cull_select_pack COMMAND test_cull_select_pack)

add_executable(test_cull_owner test_cull_owner.cc)
target_include_directories(test_cull_owner PRIVATE "./")
target_link_libraries(test_cull_owner PRIVATE cull uti commlists ${SGE_LIBS})
add_test(NAME test_cull_owner COMMAND test_cull_owner)

add_executable(test_cull_pack_format test_cull_pack_format.cc)
target_include_directories(test_cull_pack_format PRIVATE "./")
target_link_libraries(test_cull_pack_format PRIVATE cull uti commlists ${SGE_LIBS})
add_test(NAME test_cull_pack_format COMMAND test_cull_pack_format)

add_executable(test_cull_lazy test_cull_lazy.cc)
target_include_directories(test_cull_lazy PRIVATE "./")
target_link_libraries(test_cull_lazy PRIVATE cull uti commlists ${SGE_LIBS})
add_test(NAME test_cull_lazy COMMAND test_cull_lazy)

add_executable(test_cull_descr test_cull_descr.cc)
target_include_directories(test_cull_descr PRIVATE "./")
target_link_libraries(test_cull_descr PRIVATE cull uti commlists ${SGE_LIBS})
add_test(NAME test_cull_descr COMMAND test_cull_descr)

# CS-2993: the counters exist only in a statistics build
if (ENABLE_CULL_STATS)
   add_executable(test_cull_stats test_cull_stats.cc)
   target_include_directories(test_cull_stats PRIVATE "./")
   target_link_libraries(test_cull_stats PRIVATE cull uti commlists ${SGE_LIBS})
   add_test(NAME test_cull_stats COMMAND test_cull_stats)
endif ()

add_executable(test_ocs_cull_perf test_ocs_cull_perf.cc)
target_include_directories(test_ocs_cull_perf PRIVATE "./")
target_link_libraries(test_ocs_cull_perf PRIVATE cull sgeobj uti commlists ${SGE_LIBS})
add_test(NAME test_ocs_cull_perf COMMAND test_ocs_cull_perf)

# test_ocs_cull_perf and test_cull_hash are correct but long-running, so they are
# kept out of the default ctest run. Opt in by reconfiguring with
# -DENABLE_LONG_TESTS=ON (same switch as the uti lock/deadlock tests).
if (NOT ENABLE_LONG_TESTS)
   set_tests_properties(test_ocs_cull_perf test_cull_hash PROPERTIES DISABLED TRUE)
endif ()

if (INSTALL_SGE_TEST)
   install(TARGETS test_ocs_cull_perf DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_hash DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_list DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_observe DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_pack DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_enumeration DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_no_transfer DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_sort DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_access DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_lookup DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_db DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_select_pack DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_owner DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_pack_format DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_lazy DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_cull_descr DESTINATION testbin/${SGE_ARCH})
   if (ENABLE_CULL_STATS)
      install(TARGETS test_cull_stats DESTINATION testbin/${SGE_ARCH})
   endif ()
endif ()
