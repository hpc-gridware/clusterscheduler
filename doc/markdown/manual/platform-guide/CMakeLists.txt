#___INFO__MARK_BEGIN_NEW__
###########################################################################
#  
#  Copyright 2025 HPC-Gridware GmbH
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

# clusterscheduler/doc/markdown/manual/platform-guide

# build platform guide
build_markdown_manual("platform-guide" "Platform Guide" "0")

# target for building platform guide
add_custom_target(platformguide ALL DEPENDS
      ${CMAKE_CURRENT_BINARY_DIR}/platform-guide.pdf
)

# installation
if (INSTALL_SGE_DOC)
   install(FILES ${CMAKE_CURRENT_BINARY_DIR}/platform-guide.pdf DESTINATION doc/pdf)
endif ()
