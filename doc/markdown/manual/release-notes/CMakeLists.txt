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

# clusterscheduler/doc/markdown/manual/release-notes

# build release notes
build_markdown_manual("release-notes" "Release Notes")

# target for building release notes
add_custom_target(releasenotes ALL DEPENDS
      ${CMAKE_CURRENT_BINARY_DIR}/release-notes.pdf
      #        ${CMAKE_CURRENT_BINARY_DIR}/html
)

# installation
if (INSTALL_SGE_DOC)
   install(FILES ${CMAKE_CURRENT_BINARY_DIR}/release-notes.pdf DESTINATION doc/pdf)
   #   install(FILES ${CMAKE_CURRENT_BINARY_DIR}/html/* DESTINATION doc/html/user-guide)
endif ()
