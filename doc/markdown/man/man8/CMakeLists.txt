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

# clusterscheduler/doc/markdown/man/man8

# build all man pages from section 8
set(PAGES sge_execd sge_qmaster sge_shadowd sge_shepherd)
build_markdown_man("8" PAGES "0")

# target for building all troff man pages from section 8
# @todo is there a better way of listing them? Didn't find an easily comprehensible way to use foreach().
add_custom_target(troffman8 ALL DEPENDS
      ${CMAKE_CURRENT_BINARY_DIR}/sge_execd.8
      ${CMAKE_CURRENT_BINARY_DIR}/sge_qmaster.8
      ${CMAKE_CURRENT_BINARY_DIR}/sge_shadowd.8
      ${CMAKE_CURRENT_BINARY_DIR}/sge_shepherd.8
)

# installation
if (INSTALL_SGE_DOC)
   foreach (PAGE ${PAGES})
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PAGE}.8 DESTINATION man/man8)
   endforeach ()
endif ()
