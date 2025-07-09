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

# clusterscheduler/doc/markdown/man/man1

# build special man pages including submit.md
set(SUBMIT_PAGES qalter qlogin qresub qrsh qsh qsub submit)
build_markdown_man_from_template("1" "submit.include" SUBMIT_PAGES "0")

set(HOSTNAME_PAGES gethostbyaddr gethostbyname gethostname getservbyname sge_hostnameutils)
build_markdown_man_from_template("1" "gethost.include" HOSTNAME_PAGES "0")

# build all other man pages from section 1
set(PAGES qacct qconf qdel qhold qhost qmake qmod qping qquota qrdel qrls qrstat qrsub qselect qstat sge_ckpt sge_intro sge_jsv sge_types sge_share_mon)
build_markdown_man("1" PAGES "0")

# target for building all troff man pages from section 1
# @todo is there a better way of listing them? Didn't find an easily comprehensible way to use foreach().
add_custom_target(troffman1 ALL DEPENDS
      # man pages depending on submit.include.md
      ${CMAKE_CURRENT_BINARY_DIR}/qalter.1
      ${CMAKE_CURRENT_BINARY_DIR}/qlogin.1
      ${CMAKE_CURRENT_BINARY_DIR}/qresub.1
      ${CMAKE_CURRENT_BINARY_DIR}/qrsh.1
      ${CMAKE_CURRENT_BINARY_DIR}/qsh.1
      ${CMAKE_CURRENT_BINARY_DIR}/qsub.1
      ${CMAKE_CURRENT_BINARY_DIR}/submit.1

      # man pages depending on hostname.include.md
      ${CMAKE_CURRENT_BINARY_DIR}/gethostbyaddr.1
      ${CMAKE_CURRENT_BINARY_DIR}/gethostbyname.1
      ${CMAKE_CURRENT_BINARY_DIR}/gethostname.1
      ${CMAKE_CURRENT_BINARY_DIR}/getservbyname.1
      ${CMAKE_CURRENT_BINARY_DIR}/sge_hostnameutils.1

      # all other man pages of section 1
      ${CMAKE_CURRENT_BINARY_DIR}/qacct.1
      ${CMAKE_CURRENT_BINARY_DIR}/qconf.1
      ${CMAKE_CURRENT_BINARY_DIR}/qdel.1
      ${CMAKE_CURRENT_BINARY_DIR}/qhold.1
      ${CMAKE_CURRENT_BINARY_DIR}/qhost.1
      ${CMAKE_CURRENT_BINARY_DIR}/qmake.1
      ${CMAKE_CURRENT_BINARY_DIR}/qmod.1
      ${CMAKE_CURRENT_BINARY_DIR}/qping.1
      ${CMAKE_CURRENT_BINARY_DIR}/qquota.1
      ${CMAKE_CURRENT_BINARY_DIR}/qrdel.1
      ${CMAKE_CURRENT_BINARY_DIR}/qrls.1
      ${CMAKE_CURRENT_BINARY_DIR}/qrstat.1
      ${CMAKE_CURRENT_BINARY_DIR}/qrsub.1
      ${CMAKE_CURRENT_BINARY_DIR}/qselect.1
      ${CMAKE_CURRENT_BINARY_DIR}/qstat.1
      ${CMAKE_CURRENT_BINARY_DIR}/sge_ckpt.1 # @todo move to section 5?
      ${CMAKE_CURRENT_BINARY_DIR}/sge_intro.1
      ${CMAKE_CURRENT_BINARY_DIR}/sge_jsv.1
      ${CMAKE_CURRENT_BINARY_DIR}/sge_share_mon.1
      ${CMAKE_CURRENT_BINARY_DIR}/sge_types.1
)

# installation
if (INSTALL_SGE_DOC)
   foreach (PAGE ${SUBMIT_PAGES} ${HOSTNAME_PAGES} ${PAGES})
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PAGE}.1 DESTINATION man/man1)
   endforeach ()
endif ()
