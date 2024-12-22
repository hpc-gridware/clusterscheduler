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

# source

# sgeobj library needs to be first, as it contains the generation of CULL header files required by others
add_subdirectory(libs)

add_subdirectory(3rdparty)
add_subdirectory(clients)
add_subdirectory(common)
add_subdirectory(daemons)
add_subdirectory(dist)
add_subdirectory(security)
add_subdirectory(utilbin)

if (INSTALL_SGE_COMMON)
   install(
         DIRECTORY scripts/dtrace
         DESTINATION .
         USE_SOURCE_PERMISSIONS)
endif ()
