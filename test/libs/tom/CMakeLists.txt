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

# test/libs/tom: module tests of the typed object model (CS-3043)

add_executable(test_tom_traits test_tom_traits.cc)
target_include_directories(test_tom_traits PRIVATE "./")
target_link_libraries(test_tom_traits PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_traits COMMAND test_tom_traits)

add_executable(test_tom_handles test_tom_handles.cc)
target_include_directories(test_tom_handles PRIVATE "./")
target_link_libraries(test_tom_handles PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_handles COMMAND test_tom_handles)

add_executable(test_tom_list test_tom_list.cc)
target_include_directories(test_tom_list PRIVATE "./")
target_link_libraries(test_tom_list PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_list COMMAND test_tom_list)

add_executable(test_tom_legacy test_tom_legacy.cc)
target_include_directories(test_tom_legacy PRIVATE "./")
target_link_libraries(test_tom_legacy PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_legacy COMMAND test_tom_legacy)

add_executable(test_tom_unique test_tom_unique.cc)
target_include_directories(test_tom_unique PRIVATE "./")
target_link_libraries(test_tom_unique PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_unique COMMAND test_tom_unique)

add_executable(test_tom_soft_path test_tom_soft_path.cc)
target_include_directories(test_tom_soft_path PRIVATE "./")
target_link_libraries(test_tom_soft_path PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_soft_path COMMAND test_tom_soft_path)

add_executable(test_tom_ops_list test_tom_ops_list.cc)
target_include_directories(test_tom_ops_list PRIVATE "./")
target_link_libraries(test_tom_ops_list PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_ops_list COMMAND test_tom_ops_list)

add_executable(test_tom_ops_own test_tom_ops_own.cc)
target_include_directories(test_tom_ops_own PRIVATE "./")
target_link_libraries(test_tom_ops_own PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_ops_own COMMAND test_tom_ops_own)

add_executable(test_tom_ops_lookup test_tom_ops_lookup.cc)
target_include_directories(test_tom_ops_lookup PRIVATE "./")
target_link_libraries(test_tom_ops_lookup PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_ops_lookup COMMAND test_tom_ops_lookup)

add_executable(test_tom_ops_keyed test_tom_ops_keyed.cc)
target_include_directories(test_tom_ops_keyed PRIVATE "./")
target_link_libraries(test_tom_ops_keyed PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_ops_keyed COMMAND test_tom_ops_keyed)

add_executable(test_tom_ops_incr test_tom_ops_incr.cc)
target_include_directories(test_tom_ops_incr PRIVATE "./")
target_link_libraries(test_tom_ops_incr PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_ops_incr COMMAND test_tom_ops_incr)

add_executable(test_tom_ops_select test_tom_ops_select.cc)
target_include_directories(test_tom_ops_select PRIVATE "./")
target_link_libraries(test_tom_ops_select PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_ops_select COMMAND test_tom_ops_select)

add_executable(test_tom_ops_sort test_tom_ops_sort.cc)
target_include_directories(test_tom_ops_sort PRIVATE "./")
target_link_libraries(test_tom_ops_sort PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_ops_sort COMMAND test_tom_ops_sort)
add_executable(test_tom_ops_pack test_tom_ops_pack.cc)
target_include_directories(test_tom_ops_pack PRIVATE "./")
target_link_libraries(test_tom_ops_pack PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_ops_pack COMMAND test_tom_ops_pack)
add_executable(test_tom_ops_dump test_tom_ops_dump.cc)
target_include_directories(test_tom_ops_dump PRIVATE "./")
target_link_libraries(test_tom_ops_dump PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_ops_dump COMMAND test_tom_ops_dump)
add_executable(test_tom_ops_rest test_tom_ops_rest.cc)
target_include_directories(test_tom_ops_rest PRIVATE "./")
target_link_libraries(test_tom_ops_rest PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_ops_rest COMMAND test_tom_ops_rest)

add_executable(test_tom_ops_free test_tom_ops_free.cc)
target_include_directories(test_tom_ops_free PRIVATE "./")
target_link_libraries(test_tom_ops_free PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_ops_free COMMAND test_tom_ops_free)

add_executable(test_tom_ops_layout test_tom_ops_layout.cc)
target_include_directories(test_tom_ops_layout PRIVATE "./")
target_link_libraries(test_tom_ops_layout PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_ops_layout COMMAND test_tom_ops_layout)

add_executable(test_tom_ops_atomic test_tom_ops_atomic.cc)
target_include_directories(test_tom_ops_atomic PRIVATE "./")
target_link_libraries(test_tom_ops_atomic PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_ops_atomic COMMAND test_tom_ops_atomic)

# TRT-02, WHT-01 and OPS-01: sources that must not compile. Each is compiled with the flags of
# test_tom_handles as part of the build, which fails if one compiles or fails
# with another error than the one it names; test_tom_handles holds the correct
# counterpart of each.
string(TOUPPER "${CMAKE_BUILD_TYPE}" build_type_upper)
foreach (name string_into_number job_field_on_queue raw_pointer_assigned setter_on_const const_to_mutable_pointer
               what_two_types what_nested_not_a_list where_foreign_field where_number_with_string
               where_nested_not_a_list sort_foreign_field sort_order_foreign_field insert_sorted_wrong_type
               pack_what_of_other_type unpack_into_wrong_handle
               undump_into_wrong_handle unique_by_number_field free_key_of_other_type
               free_key_not_of_sub_type free_bits_on_64_bit_field free_handle_foreign_field
               set_atomic_on_hashed_field)
   set(stamp ${CMAKE_CURRENT_BINARY_DIR}/compile_fail_${name}.stamp)
   add_custom_command(OUTPUT ${stamp}
           COMMAND ${CMAKE_COMMAND}
                   -DCOMPILER=${CMAKE_CXX_COMPILER}
                   "-DFLAGS=${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${build_type_upper}}"
                   -DSTANDARD=${CMAKE_CXX_STANDARD}
                   "-DINCLUDES=$<TARGET_PROPERTY:test_tom_handles,INCLUDE_DIRECTORIES>"
                   "-DDEFINES=$<TARGET_PROPERTY:test_tom_handles,COMPILE_DEFINITIONS>"
                   -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/compile_fail/${name}.cc
                   -DSTAMP=${stamp}
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/expect_compile_failure.cmake
           DEPENDS compile_fail/${name}.cc expect_compile_failure.cmake test_tom_handles
           COMMENT "Checking that compile_fail/${name}.cc does not compile"
           VERBATIM)
   add_custom_target(test_tom_compile_fail_${name} ALL DEPENDS ${stamp})
endforeach ()

# N-L-1, N-L-5: the core calls no function of the old interface and includes only tom/ and uti/; checked on every build
file(GLOB tom_core_sources "${CMAKE_SOURCE_DIR}/source/libs/tom/*.h" "${CMAKE_SOURCE_DIR}/source/libs/tom/*.cc")
set(core_calls_stamp ${CMAKE_CURRENT_BINARY_DIR}/check_core_calls.stamp)
add_custom_command(OUTPUT ${core_calls_stamp}
        COMMAND ${CMAKE_COMMAND} -DTOM_DIR=${CMAKE_SOURCE_DIR}/source/libs/tom -DSTAMP=${core_calls_stamp}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/check_core_calls.cmake
        DEPENDS ${tom_core_sources} check_core_calls.cmake
        COMMENT "Checking that the core of the typed object model does not depend on the old interface"
        VERBATIM)
add_custom_target(test_tom_core_calls ALL DEPENDS ${core_calls_stamp})

if (INSTALL_SGE_TEST)
   install(TARGETS test_tom_traits DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_handles DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_list DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_legacy DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_unique DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_soft_path DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_ops_list DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_ops_own DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_ops_lookup DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_ops_keyed DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_ops_incr DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_ops_select DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_ops_sort DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_ops_pack DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_ops_dump DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_ops_rest DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_ops_free DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_ops_layout DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_ops_atomic DESTINATION testbin/${SGE_ARCH})
endif ()
