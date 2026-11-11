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

# TRT-02: sources that must not compile. Each is compiled with the flags of
# test_tom_handles as part of the build, which fails if one compiles or fails
# with another error than the one it names; test_tom_handles holds the correct
# counterpart of each.
string(TOUPPER "${CMAKE_BUILD_TYPE}" build_type_upper)
foreach (name string_into_number job_field_on_queue raw_pointer_assigned setter_on_const const_to_mutable_pointer)
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

if (INSTALL_SGE_TEST)
   install(TARGETS test_tom_traits DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_handles DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_list DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_legacy DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_unique DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_tom_soft_path DESTINATION testbin/${SGE_ARCH})
endif ()
