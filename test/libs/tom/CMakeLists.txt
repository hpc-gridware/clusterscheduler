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
target_link_libraries(test_tom_traits PRIVATE sgeobj tom comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_traits COMMAND test_tom_traits)

add_executable(test_tom_handles test_tom_handles.cc)
target_include_directories(test_tom_handles PRIVATE "./")
target_link_libraries(test_tom_handles PRIVATE sgeobj tom comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_handles COMMAND test_tom_handles)

add_executable(test_tom_list test_tom_list.cc)
target_include_directories(test_tom_list PRIVATE "./")
target_link_libraries(test_tom_list PRIVATE sgeobj tom comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_list COMMAND test_tom_list)

add_executable(test_tom_unique test_tom_unique.cc)
target_include_directories(test_tom_unique PRIVATE "./")
target_link_libraries(test_tom_unique PRIVATE sgeobj tom comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_unique COMMAND test_tom_unique)

add_executable(test_tom_soft_path test_tom_soft_path.cc)
target_include_directories(test_tom_soft_path PRIVATE "./")
target_link_libraries(test_tom_soft_path PRIVATE sgeobj tom comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_soft_path COMMAND test_tom_soft_path)

add_executable(test_tom_free test_tom_free.cc)
target_include_directories(test_tom_free PRIVATE "./")
target_link_libraries(test_tom_free PRIVATE sgeobj tom comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_free COMMAND test_tom_free)

add_executable(test_tom_ops_atomic test_tom_ops_atomic.cc)
target_include_directories(test_tom_ops_atomic PRIVATE "./")
target_link_libraries(test_tom_ops_atomic PRIVATE sgeobj tom comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_ops_atomic COMMAND test_tom_ops_atomic)

add_executable(test_tom_model test_tom_model.cc)
target_include_directories(test_tom_model PRIVATE "./")
target_link_libraries(test_tom_model PRIVATE sgeobj tom comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_model COMMAND test_tom_model)

add_executable(test_tom_raw_access test_tom_raw_access.cc)
target_include_directories(test_tom_raw_access PRIVATE "./")
target_link_libraries(test_tom_raw_access PRIVATE sgeobj tom comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_raw_access COMMAND test_tom_raw_access)

add_executable(test_tom_raw_ops test_tom_raw_ops.cc)
target_include_directories(test_tom_raw_ops PRIVATE "./")
target_link_libraries(test_tom_raw_ops PRIVATE sgeobj tom comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_raw_ops COMMAND test_tom_raw_ops)

add_executable(test_tom_raw_build test_tom_raw_build.cc)
target_include_directories(test_tom_raw_build PRIVATE "./")
target_link_libraries(test_tom_raw_build PRIVATE sgeobj tom comm commlists uti ${SGE_LIBS})
add_test(NAME test_tom_raw_build COMMAND test_tom_raw_build)

# TRT-02, WHT-01, OPS-01, RAW-ACC, RAW-OPS and CS-3103: sources that must not compile. Each is compiled with the
# flags of test_tom_handles as part of the build, which fails if one compiles or fails
# with another error than the one it names; test_tom_handles holds the correct
# counterpart of each.
string(TOUPPER "${CMAKE_BUILD_TYPE}" build_type_upper)
foreach (name string_into_number job_field_on_queue raw_pointer_assigned setter_on_const const_to_mutable_pointer
               what_two_types what_nested_not_a_list where_foreign_field where_number_with_string
               where_nested_not_a_list sort_foreign_field sort_order_foreign_field insert_sorted_wrong_type
               pack_what_of_other_type unpack_into_wrong_handle
               undump_into_wrong_handle unique_by_number_field free_key_of_other_type
               free_key_not_of_sub_type free_bits_on_64_bit_field free_handle_foreign_field
               set_atomic_on_hashed_field get_atomic_on_32_bit_field set_atomic_on_const hash_on_bool_field
               pack_type_with_int_field merge_by_key_double_key
               raw_set_std_string raw_get_mutable_from_const raw_handle_mutable_from_const raw_owner_copied
               raw_set_typed_owner member_bits_on_64_bit_field member_or_bits_on_const
               free_get_on_handle free_has_on_handle free_set_on_handle free_get_on_const_handle
               foreign_list_into_typed_field foreign_element_into_typed_field get_as_on_typed_field
               free_find_first_rw_on_const_list free_find_next_on_typed_list
               free_exchange_on_handle free_exchange_on_owner free_get_or_create_on_handle
               free_runtime_get_on_handle free_runtime_set_on_handle free_runtime_has_on_handle
               free_has_typed_flag_on_handle free_list_ref_typed_flag_on_handle
               free_find_first_in_on_handle free_find_first_in_rw_on_handle free_emplace_back_in_on_handle
               free_erase_in_on_handle free_swap_on_handle free_list_ref_on_handle free_get_or_empty_on_handle
               free_has_any_bits_on_handle free_or_bits_on_handle free_and_bits_on_handle free_clear_bits_on_handle
               free_runtime_get_or_on_handle free_runtime_exchange_on_handle free_runtime_take_on_handle)
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
   install(TARGETS test_tom_unique DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_soft_path DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_free DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_ops_atomic DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_model DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_raw_access DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_raw_ops DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_raw_build DESTINATION testbin/${SGE_ARCH})
endif ()

# the tests of the list core, formerly the CULL tests (CS-3103)
add_subdirectory(core)
