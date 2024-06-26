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

function(build_markdown_man section files)
   string(TIMESTAMP DATETIME "%Y-%m-%d")
   foreach (page IN LISTS ${files})
      add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${page}.${section}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${page}.md
            COMMAND ${CMAKE_SOURCE_DIR}/doc/scripts/pandoc_man.sh ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${page} "NONE" ${section} ${CMAKE_PROJECT_VERSION} ${DATETIME}
      )
   endforeach ()
endfunction()

function(build_markdown_man_from_template section template files)
   string(TIMESTAMP DATETIME "%Y-%m-%d")
   foreach (page IN LISTS ${files})
      add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${page}.${section}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${page}.md ${CMAKE_CURRENT_SOURCE_DIR}/${template}.md
            COMMAND ${CMAKE_SOURCE_DIR}/doc/scripts/pandoc_man.sh ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${page} ${template} ${section} ${CMAKE_PROJECT_VERSION} ${DATETIME}
      )
   endforeach ()
endfunction()

