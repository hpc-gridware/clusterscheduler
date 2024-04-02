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

# source/clients/qevent
add_executable(
      qevent oge_qevent.cc
      ../../common/sig_handlers.cc
      ../../common/sge_options.cc
      ../../common/usage.cc)
target_include_directories(qevent PUBLIC ${SGE_INCLUDES})
target_link_libraries(
      qevent
      PUBLIC clientscommon
      comm
      commlists
      mir
      evc
      gdi
      sgeobj
      cull
      uti
      ${SGE_LIBS})

if (INSTALL_SGE_TEST)
   install(TARGETS qevent DESTINATION testbin/${SGE_ARCH})
endif ()
