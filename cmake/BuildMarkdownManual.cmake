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

function(build_markdown_manual file title located_in_extensions)
   string(TIMESTAMP DATETIME "%Y-%m-%d")
   add_custom_command(
         OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${file}.pdf
         DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.md
         ${CMAKE_SOURCE_DIR}/../clusterscheduler/doc/markdown/manual/titlepage.md
         ${CMAKE_SOURCE_DIR}/../clusterscheduler/doc/markdown/manual/copyright.md
         ${CMAKE_SOURCE_DIR}/../clusterscheduler/doc/markdown/manual/typographic_conventions.md
         ${CMAKE_SOURCE_DIR}/../clusterscheduler/doc/markdown/manual/head.tex
         ${CMAKE_SOURCE_DIR}/../clusterscheduler/doc/scripts/pandoc_manual.sh
         COMMAND ${CMAKE_SOURCE_DIR}/doc/scripts/pandoc_manual.sh ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
         ${CMAKE_CURRENT_BINARY_DIR} ${file} ${title} ${CMAKE_PROJECT_VERSION} ${DATETIME} ${located_in_extensions}
   )
endfunction()
