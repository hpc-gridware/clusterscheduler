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

# test/daemons/execd


# CS-3209: lines of any length from a load sensor no longer overflow a stack buffer
add_executable(test_execd_load_sensor
      test_execd_load_sensor.cc
      ../../../source/daemons/execd/ocs_load_sensor_line.cc)
target_include_directories(test_execd_load_sensor PRIVATE "./" "../../../source/daemons/execd")
target_link_libraries(test_execd_load_sensor PRIVATE uti ${SGE_LIBS})
add_test(NAME test_execd_load_sensor COMMAND test_execd_load_sensor)

if (INSTALL_SGE_TEST)
   install(TARGETS test_execd_load_sensor DESTINATION testbin/${SGE_ARCH})
endif ()
