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

# source/utilbin

# adminrun
add_executable(adminrun adminrun.cc)
target_include_directories(adminrun PRIVATE ${SGE_INCLUDES})
target_link_libraries(adminrun PRIVATE commlists uti ${SGE_LIBS})

# checkprog
add_executable(checkprog checkprog.cc)
target_include_directories(checkprog PRIVATE ${SGE_INCLUDES})
target_link_libraries(checkprog PRIVATE commlists uti ${SGE_LIBS})

# checkuser
add_executable(checkuser checkuser.cc)
target_include_directories(checkuser PRIVATE ${SGE_INCLUDES})
target_link_libraries(checkuser PRIVATE commlists uti ${SGE_LIBS})

# echo_raw
add_executable(echo_raw echo_raw.cc)
target_include_directories(echo_raw PRIVATE ${SGE_INCLUDES})
target_link_libraries(echo_raw PRIVATE commlists uti ${SGE_LIBS})

# filestat
add_executable(filestat filestat.cc)
target_include_directories(filestat PRIVATE ${SGE_INCLUDES})
target_link_libraries(filestat PRIVATE commlists uti ${SGE_LIBS})

# fstype
add_executable(fstype fstype.cc)
target_include_directories(fstype PRIVATE ${SGE_INCLUDES})
target_link_libraries(fstype PRIVATE commlists uti ${SGE_LIBS})

# gethostbyaddr
add_executable(gethostbyaddr gethostbyaddr.cc)
target_include_directories(gethostbyaddr PRIVATE ${SGE_INCLUDES})
target_link_libraries(gethostbyaddr PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})

# gethostbyname
add_executable(gethostbyname gethostbyname.cc)
target_include_directories(gethostbyname PRIVATE ${SGE_INCLUDES})
target_link_libraries(gethostbyname PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})

# gethostname
add_executable(gethostname gethostname.cc)
target_include_directories(gethostname PRIVATE ${SGE_INCLUDES})
target_link_libraries(gethostname PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})

# getservbyname
add_executable(getservbyname getservbyname.cc)
target_include_directories(getservbyname PRIVATE ${SGE_INCLUDES})
target_link_libraries(getservbyname PRIVATE sgeobj cull comm commlists uti ${SGE_LIBS})

# infotext
add_executable(infotext infotext.cc)
target_include_directories(infotext PRIVATE ${SGE_INCLUDES})
target_link_libraries(infotext PRIVATE sgeobj cull commlists uti ${SGE_LIBS})

# loadcheck
add_executable(loadcheck
      loadcheck.cc
      ../../../${PROJECT_FEATURES}/source/daemons/qmaster/TestClass.cc
)
target_include_directories(loadcheck PRIVATE ${SGE_INCLUDES})
target_link_libraries(loadcheck PRIVATE sgeobj cull commlists uti
      ${SGE_TOPO_LIB} ${SGE_LIBS})

# now
add_executable(now now.cc)
target_include_directories(now PRIVATE ${SGE_INCLUDES})

# qping
add_executable(qping qping.cc)
target_include_directories(qping PRIVATE $SGE_INCLUDES)
target_link_libraries(
      qping
      PRIVATE gdi
      sgeobj
      cull
      comm
      commlists
      uti
      ${SGE_LIBS})

# read_raw
add_executable(read_raw read_raw.cc)
target_include_directories(read_raw PRIVATE ${SGE_INCLUDES})

# sge_share_mon
add_executable(sge_share_mon sge_share_mon.cc)
target_include_directories(sge_share_mon PRIVATE ${SGE_INCLUDES})
target_link_libraries(
      sge_share_mon
      PRIVATE sched
      gdi
      sgeobj
      cull
      comm
      commlists
      uti
      ${SGE_LIBS})

# spooldefaults
add_executable(spooldefaults spooldefaults.cc)
target_include_directories(spooldefaults PRIVATE ${SGE_INCLUDES})
target_link_libraries(
      spooldefaults
      PRIVATE
      spoolc
      ${SPOOLING_LIBS}
      sched
      gdi
      sgeobj
      cull
      comm
      commlists
      uti
      ${SGE_LIBS})

# spooledit
add_executable(spooledit spooledit.cc)
target_include_directories(spooledit PRIVATE ${SGE_INCLUDES})
target_link_libraries(
      spooledit
      PRIVATE
      #spoolc
      ${SPOOLING_LIBS}
      sched
      gdi
      sgeobj
      cull
      comm
      commlists
      uti
      ${SGE_LIBS})

# spoolinit
add_executable(spoolinit spoolinit.cc)
target_include_directories(spoolinit PRIVATE ${SGE_INCLUDES})
target_link_libraries(
      spoolinit
      PRIVATE
      #spoolc
      ${SPOOLING_LIBS}
      sched
      sgeobj
      gdi
      cull
      comm
      commlists
      uti
      ${SGE_LIBS})

# testsuidroot
add_executable(testsuidroot testsuidroot.cc)
target_include_directories(testsuidroot PRIVATE ${SGE_INCLUDES})
target_link_libraries(testsuidroot PRIVATE commlists uti ${SGE_LIBS})

# required to disable warning for deprecated rresvport() function
set_source_files_properties(testsuidroot.cc PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)

# uidgid
add_executable(uidgid uidgid.cc)
target_include_directories(uidgid PRIVATE ${SGE_INCLUDES})
target_link_libraries(uidgid PRIVATE commlists uti ${SGE_LIBS})

# work
add_executable(work work.cc)
target_include_directories(work PRIVATE ${SGE_INCLUDES})
target_link_libraries(
      work
      PRIVATE gdi
      sgeobj
      cull
      comm
      commlists
      uti
      ${SGE_LIBS})

if (INSTALL_SGE_BIN_CLIENT)
   install(TARGETS qping DESTINATION bin/${SGE_ARCH})
   install(
         TARGETS adminrun
         checkprog
         checkuser
         echo_raw
         filestat
         fstype
         gethostbyaddr
         gethostbyname
         gethostname
         getservbyname
         infotext
         loadcheck
         now
         read_raw
         sge_share_mon
         testsuidroot
         uidgid
         DESTINATION utilbin/${SGE_ARCH})
endif ()

if (INSTALL_SGE_BIN_MASTER)
   install(
           TARGETS
           spooldefaults
           spooledit
           spoolinit
           DESTINATION utilbin/${SGE_ARCH})
   install(TARGETS work DESTINATION examples/jobsbin/${SGE_ARCH})
endif ()
