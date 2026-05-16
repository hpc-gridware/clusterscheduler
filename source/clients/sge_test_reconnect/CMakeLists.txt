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

# source/clients/sge_test_reconnect
add_executable(sge_test_reconnect sge_test_reconnect.cc ../../common/sig_handlers.cc
      ../../common/sge_options.cc ../../common/usage.cc)
target_include_directories(sge_test_reconnect PRIVATE ${SGE_INCLUDES})
target_link_libraries(
      sge_test_reconnect
      PRIVATE gdi
      sgeobj
      cull
      comm
      commlists
      uti
      ${SGE_LIBS})

if (INSTALL_SGE_BIN_CLIENT)
   install(TARGETS sge_test_reconnect DESTINATION bin/${SGE_ARCH})
endif ()
