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

# test/libs/tom/core

add_executable(test_tom_core_hash test_tom_core_hash.cc)
target_include_directories(test_tom_core_hash PRIVATE "./")
target_link_libraries(test_tom_core_hash PRIVATE tom uti commlists ${SGE_LIBS})
add_test(NAME test_tom_core_hash COMMAND test_tom_core_hash)

add_executable(test_tom_core_list test_tom_core_list.cc)
target_include_directories(test_tom_core_list PRIVATE "./")
target_link_libraries(test_tom_core_list PRIVATE tom uti commlists ${SGE_LIBS})
add_test(NAME test_tom_core_list COMMAND test_tom_core_list)

add_executable(test_tom_core_pack test_tom_core_pack.cc)
target_include_directories(test_tom_core_pack PRIVATE "./")
target_link_libraries(test_tom_core_pack PRIVATE tom uti commlists ${SGE_LIBS})
add_test(NAME test_tom_core_pack COMMAND test_tom_core_pack)

add_executable(test_tom_core_no_transfer test_tom_core_no_transfer.cc)
target_include_directories(test_tom_core_no_transfer PRIVATE "./")
target_link_libraries(test_tom_core_no_transfer PRIVATE tom uti commlists ${SGE_LIBS})
add_test(NAME test_tom_core_no_transfer COMMAND test_tom_core_no_transfer)

add_executable(test_tom_core_enumeration test_tom_core_enumeration.cc)
target_include_directories(test_tom_core_enumeration PRIVATE "./")
target_link_libraries(test_tom_core_enumeration PRIVATE tom uti commlists ${SGE_LIBS})
add_test(NAME test_tom_core_enumeration COMMAND test_tom_core_enumeration)

add_executable(test_tom_core_sort test_tom_core_sort.cc)
target_include_directories(test_tom_core_sort PRIVATE "./")
target_link_libraries(test_tom_core_sort PRIVATE tom uti commlists ${SGE_LIBS})
add_test(NAME test_tom_core_sort COMMAND test_tom_core_sort)

add_executable(test_tom_core_access test_tom_core_access.cc)
target_include_directories(test_tom_core_access PRIVATE "./")
target_link_libraries(test_tom_core_access PRIVATE tom uti commlists ${SGE_LIBS})
add_test(NAME test_tom_core_access COMMAND test_tom_core_access)

add_executable(test_tom_core_lookup test_tom_core_lookup.cc)
target_include_directories(test_tom_core_lookup PRIVATE "./")
target_link_libraries(test_tom_core_lookup PRIVATE tom uti commlists ${SGE_LIBS})
add_test(NAME test_tom_core_lookup COMMAND test_tom_core_lookup)

add_executable(test_tom_core_db test_tom_core_db.cc)
target_include_directories(test_tom_core_db PRIVATE "./")
target_link_libraries(test_tom_core_db PRIVATE tom uti commlists ${SGE_LIBS})
add_test(NAME test_tom_core_db COMMAND test_tom_core_db)

add_executable(test_tom_core_select_pack test_tom_core_select_pack.cc)
target_include_directories(test_tom_core_select_pack PRIVATE "./")
target_link_libraries(test_tom_core_select_pack PRIVATE tom uti commlists ${SGE_LIBS})
add_test(NAME test_tom_core_select_pack COMMAND test_tom_core_select_pack)

add_executable(test_tom_core_owner test_tom_core_owner.cc)
target_include_directories(test_tom_core_owner PRIVATE "./")
target_link_libraries(test_tom_core_owner PRIVATE tom uti commlists ${SGE_LIBS})
add_test(NAME test_tom_core_owner COMMAND test_tom_core_owner)

add_executable(test_tom_core_pack_format test_tom_core_pack_format.cc)
target_include_directories(test_tom_core_pack_format PRIVATE "./")
target_link_libraries(test_tom_core_pack_format PRIVATE tom uti commlists ${SGE_LIBS})
add_test(NAME test_tom_core_pack_format COMMAND test_tom_core_pack_format)

add_executable(test_tom_core_lazy test_tom_core_lazy.cc)
target_include_directories(test_tom_core_lazy PRIVATE "./")
target_link_libraries(test_tom_core_lazy PRIVATE tom uti commlists ${SGE_LIBS})
add_test(NAME test_tom_core_lazy COMMAND test_tom_core_lazy)

add_executable(test_tom_core_descr test_tom_core_descr.cc)
target_include_directories(test_tom_core_descr PRIVATE "./")
target_link_libraries(test_tom_core_descr PRIVATE tom uti commlists ${SGE_LIBS})
add_test(NAME test_tom_core_descr COMMAND test_tom_core_descr)

# CS-2997 SHD-05: two modules built like a spooling plugin, the second one bound to its own CULL
foreach (variant shared split)
   add_library(test_tom_core_plugin_${variant} MODULE test_tom_core_plugin_module.cc)
   target_include_directories(test_tom_core_plugin_${variant} PRIVATE "./")
   target_link_libraries(test_tom_core_plugin_${variant} PRIVATE tom uti commlists ${SGE_LIBS_SHARED})
   set_target_properties(test_tom_core_plugin_${variant} PROPERTIES PREFIX "")
   ocs_link_registry_probe(test_tom_core_plugin_${variant})
endforeach ()
target_link_options(test_tom_core_plugin_split PRIVATE -Wl,-Bsymbolic)

add_executable(test_tom_core_plugin test_tom_core_plugin.cc ../../../../source/common/ocs_SanitizerOptions.cc)
target_include_directories(test_tom_core_plugin PRIVATE "./")
target_link_libraries(test_tom_core_plugin PRIVATE tom uti commlists ${SGE_LIBS})
add_dependencies(test_tom_core_plugin test_tom_core_plugin_shared test_tom_core_plugin_split)
add_test(NAME test_tom_core_plugin COMMAND test_tom_core_plugin)

# CS-2993: the counters exist only in a statistics build
if (ENABLE_CULL_STATS)
   add_executable(test_tom_core_stats test_tom_core_stats.cc)
   target_include_directories(test_tom_core_stats PRIVATE "./")
   target_link_libraries(test_tom_core_stats PRIVATE tom uti commlists ${SGE_LIBS})
   add_test(NAME test_tom_core_stats COMMAND test_tom_core_stats)
endif ()

add_executable(test_tom_core_perf test_tom_core_perf.cc)
target_include_directories(test_tom_core_perf PRIVATE "./")
target_link_libraries(test_tom_core_perf PRIVATE tom sgeobj uti commlists ${SGE_LIBS})
add_test(NAME test_tom_core_perf COMMAND test_tom_core_perf)

# CS-3103: the old function interface of the list library is gone; the internal headers of libs/tom stay
# reserved to libs/tom, libs/sgeobj/tom and the tests (errors), and no product file calls a TOM internal
# (errors). See check_tom_api.cmake.
# Checked on every build (target check_tom_api) and by ctest (test check_tom_api).
set(tom_api_extensions_dir "")
if (NOT PROJECT_EXTENSIONS STREQUAL "None")
   get_filename_component(tom_api_extensions_dir "${PROJECT_EXTENSIONS}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
endif ()
set(tom_api_roots "${CMAKE_SOURCE_DIR}/source" "${CMAKE_SOURCE_DIR}/test")
if (tom_api_extensions_dir)
   list(APPEND tom_api_roots "${tom_api_extensions_dir}/source" "${tom_api_extensions_dir}/test")
endif ()
set(tom_api_scanned "")
foreach (root IN LISTS tom_api_roots)
   file(GLOB_RECURSE scanned CONFIGURE_DEPENDS
        "${root}/*.h" "${root}/*.hh" "${root}/*.c" "${root}/*.cc" "${root}/*.cpp")
   list(APPEND tom_api_scanned ${scanned})
endforeach ()
set(tom_api_stamp ${CMAKE_CURRENT_BINARY_DIR}/check_tom_api.stamp)
add_custom_command(OUTPUT ${tom_api_stamp}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_SOURCE_DIR} -DEXTENSIONS_DIR=${tom_api_extensions_dir}
                -DSTAMP=${tom_api_stamp} -P ${CMAKE_CURRENT_SOURCE_DIR}/check_tom_api.cmake
        DEPENDS ${tom_api_scanned} check_tom_api.cmake
        COMMENT "Checking that the old interface of the list library is included only where allowed"
        VERBATIM)
add_custom_target(check_tom_api ALL DEPENDS ${tom_api_stamp})
add_test(NAME check_tom_api
         COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_SOURCE_DIR} -DEXTENSIONS_DIR=${tom_api_extensions_dir}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/check_tom_api.cmake)

# test_tom_core_perf and test_tom_core_hash are correct but long-running, so they are
# kept out of the default ctest run. Opt in by reconfiguring with
# -DENABLE_LONG_TESTS=ON (same switch as the uti lock/deadlock tests).
if (NOT ENABLE_LONG_TESTS)
   set_tests_properties(test_tom_core_perf test_tom_core_hash PROPERTIES DISABLED TRUE)
endif ()

if (INSTALL_SGE_TEST)
   install(TARGETS test_tom_core_perf DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_core_hash DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_core_list DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_core_pack DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_core_enumeration DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_core_no_transfer DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_core_sort DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_core_access DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_core_lookup DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_core_db DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_core_select_pack DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_core_owner DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_core_pack_format DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_core_lazy DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_core_descr DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_core_plugin test_tom_core_plugin_shared test_tom_core_plugin_split DESTINATION testbin/${SGE_ARCH})
   if (ENABLE_CULL_STATS)
      install(TARGETS test_tom_core_stats DESTINATION testbin/${SGE_ARCH})
   endif ()
endif ()
