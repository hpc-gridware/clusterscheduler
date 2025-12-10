#___INFO__MARK_BEGIN_NEW__
###########################################################################
#  
#  Copyright 2024-2025 HPC-Gridware GmbH
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

# test/libs/uti

add_executable(test_uti_bitfield test_uti_bitfield.cc)
target_include_directories(test_uti_bitfield PRIVATE "./")
target_link_libraries(test_uti_bitfield PRIVATE uti commlists ${SGE_LIBS})
add_test(NAME test_uti_bitfield COMMAND test_uti_bitfield)

add_executable(test_uti_deadlock test_uti_lock_main.cc test_uti_deadlock.cc)
target_include_directories(test_uti_deadlock PRIVATE "./")
target_link_libraries(test_uti_deadlock PRIVATE uti commlists ${SGE_LIBS})
add_test(NAME test_uti_deadlock COMMAND test_uti_deadlock)

add_executable(test_uti_dstring test_uti_dstring.cc)
target_include_directories(test_uti_dstring PRIVATE "./")
target_link_libraries(test_uti_dstring PRIVATE uti commlists ${SGE_LIBS})
add_test(NAME test_uti_dstring COMMAND test_uti_dstring)

add_executable(test_uti_err test_uti_err.cc)
target_include_directories(test_uti_err PRIVATE "./")
target_link_libraries(test_uti_err PRIVATE uti commlists ${SGE_LIBS})
add_test(NAME test_uti_err COMMAND test_uti_err)

add_executable(test_uti_lock_simple test_uti_lock_main.cc test_uti_lock_simple.cc)
target_include_directories(test_uti_lock_simple PRIVATE "./")
target_link_libraries(test_uti_lock_simple PRIVATE uti commlists ${SGE_LIBS})
add_test(NAME test_uti_lock_simple COMMAND test_uti_lock_simple)

add_executable(test_uti_lock_multiple test_uti_lock_main.cc test_uti_lock_multiple.cc)
target_include_directories(test_uti_lock_multiple PRIVATE "./")
target_link_libraries(test_uti_lock_multiple PRIVATE uti commlists ${SGE_LIBS})
add_test(NAME test_uti_lock_multiple COMMAND test_uti_lock_multiple)

add_executable(test_uti_lock_fifo test_uti_lock_main.cc test_uti_lock_fifo.cc)
target_include_directories(test_uti_lock_fifo PRIVATE "./")
target_link_libraries(test_uti_lock_fifo PRIVATE uti commlists ${SGE_LIBS})
add_test(NAME test_uti_lock_fifo COMMAND test_uti_lock_fifo)

add_executable(test_uti_lock_trylock test_uti_lock_main.cc test_uti_lock_trylock.cc)
target_include_directories(test_uti_lock_trylock PRIVATE "./")
target_link_libraries(test_uti_lock_trylock PRIVATE uti commlists ${SGE_LIBS})
add_test(NAME test_uti_lock_trylock COMMAND test_uti_lock_trylock)

if (WITH_MUNGE)
   add_executable(test_uti_munge test_uti_munge.cc)
   target_include_directories(test_uti_munge PRIVATE "./")
   target_link_libraries(test_uti_munge PRIVATE uti commlists ${SGE_LIBS})
   add_test(NAME test_uti_munge COMMAND test_uti_munge)
endif()

add_executable(test_uti_os_fds test_uti_os_fds.cc)
target_include_directories(test_uti_os_fds PRIVATE "./")
target_link_libraries(test_uti_os_fds PRIVATE uti commlists ${SGE_LIBS})
add_test(NAME test_uti_os_fds COMMAND test_uti_os_fds)

add_executable(test_uti_profiling test_uti_profiling.cc)
target_include_directories(test_uti_profiling PRIVATE "./")
target_link_libraries(test_uti_profiling PRIVATE uti commlists ${SGE_LIBS})
add_test(NAME test_uti_profiling COMMAND test_uti_profiling)

add_executable(test_uti_recursive test_uti_lock_main.cc test_uti_recursive.cc)
target_include_directories(test_uti_recursive PRIVATE "./")
target_link_libraries(test_uti_recursive PRIVATE uti commlists ${SGE_LIBS})
add_test(NAME test_uti_recursive COMMAND test_uti_recursive)

add_executable(test_uti_sl test_uti_sl.cc)
target_include_directories(test_uti_sl PRIVATE "./")
target_link_libraries(test_uti_sl PRIVATE uti commlists ${SGE_LIBS})
add_test(NAME test_uti_sl COMMAND test_uti_sl)

add_executable(test_uti_string test_uti_string.cc)
target_include_directories(test_uti_string PRIVATE "./")
target_compile_options(test_uti_string PRIVATE -Wno-array-bounds)
target_link_libraries(test_uti_string PRIVATE uti commlists ${SGE_LIBS})
add_test(NAME test_uti_string COMMAND test_uti_string)

add_executable(test_uti_tq test_uti_tq.cc)
target_include_directories(test_uti_tq PRIVATE "./")
target_link_libraries(test_uti_tq PRIVATE uti commlists ${SGE_LIBS})
add_test(NAME test_uti_tq COMMAND test_uti_tq)

add_executable(test_uti_time test_uti_time.cc)
target_include_directories(test_uti_time PRIVATE "./")
target_link_libraries(test_uti_time PRIVATE uti commlists ${SGE_LIBS})
add_test(NAME test_uti_time COMMAND test_uti_time)

add_executable(test_uti_uidgid test_uti_uidgid.cc)
target_include_directories(test_uti_uidgid PRIVATE "./")
target_link_libraries(test_uti_uidgid PRIVATE uti commlists ${SGE_LIBS})
add_test(NAME test_uti_uidgid COMMAND test_uti_uidgid)

add_executable(test_uti_thread_local test_uti_thread_local.cc)
target_include_directories(test_uti_thread_local PRIVATE "./")
target_link_libraries(test_uti_thread_local PRIVATE uti commlists ${SGE_LIBS})
add_test(NAME test_uti_thread_local COMMAND test_uti_thread_local)

add_executable(test_uti_threads test_uti_threads.cc)
target_include_directories(test_uti_threads PRIVATE "./")
target_link_libraries(test_uti_threads PRIVATE uti commlists ${SGE_LIBS})
add_test(NAME test_uti_threads COMMAND test_uti_threads)

add_executable(test_uti_Encoder test_uti_Encoder.cc)
target_include_directories(test_uti_Encoder PRIVATE "./")
target_link_libraries(test_uti_Encoder PRIVATE uti commlists ${SGE_LIBS})
add_test(NAME test_uti_Encoder COMMAND test_uti_Encoder)

if (INSTALL_SGE_TEST)
   install(TARGETS test_uti_bitfield DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_uti_deadlock DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_uti_dstring DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_uti_Encoder DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_uti_err DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_uti_lock_simple DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_uti_lock_multiple DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_uti_lock_fifo DESTINATION testbin/${SGE_ARCH})
   if (WITH_MUNGE)
      install(TARGETS test_uti_munge DESTINATION testbin/${SGE_ARCH})
   endif()
   install(TARGETS test_uti_os_fds DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_uti_profiling DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_uti_recursive DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_uti_sl DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_uti_string DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_uti_time DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_uti_tq DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_uti_uidgid DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_uti_thread_local DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_uti_threads DESTINATION testbin/${SGE_ARCH})
endif ()

