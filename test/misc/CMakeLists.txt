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

# test/misc

#add_subdirectory(systemd)

#add_executable(test_valgrind test_valgrind.cc)
#add_compile_options(test_valgrind PRIVATE "-no-pedantic -Wno-error")
add_executable(test_cond_timedwait test_cond_timedwait.cc)

if (INSTALL_SGE_TEST)
#   install(TARGETS test_valgrind DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_cond_timedwait DESTINATION testbin/${SGE_ARCH})
endif ()
