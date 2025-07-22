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

# source/daemons/shepherd
add_executable(sge_shepherd
      builtin_starter.cc
      ocs_shepherd_systemd.cc
      setjoblimit.cc
      setrlimits.cc
      sge_fileio.cc
      sge_pset.cc
      sge_shepconf.cc
      sge_shepherd_ijs.cc
      shepherd.cc
      shepherd_binding.cc
      signal_queue.cc
      ../../common/execution_states.cc
      ../../common/sge_ijs_comm.cc
      ../../common/sge_ijs_threads.cc)

target_include_directories(sge_shepherd PRIVATE ${SGE_INCLUDES})

target_link_libraries(
      sge_shepherd
      PRIVATE daemonscommon
      gdi
      sgeobj
      cull
      comm
      commlists
      uti
      ${SGE_TOPO_LIB}
      ${SGE_LIBS})

if (INSTALL_SGE_BIN_EXEC)
   install(TARGETS sge_shepherd DESTINATION bin/${SGE_ARCH})
endif ()
