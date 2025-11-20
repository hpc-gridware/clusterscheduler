#___INFO__MARK_BEGIN_NEW__
###########################################################################
#  
#  Copyright 2023-2025 HPC-Gridware GmbH
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

# source/libs/spool/flatfile it is the classic spooling lib libspoolc.so
set(LIBRARY_NAME spoolc)
set(LIBRARY_SOURCES
        sge_flatfile.cc
        sge_flatfile_obj.cc
        sge_flatfile_obj_rsmap.cc
        sge_spooling_flatfile.cc
        sge_spooling_flatfile_scanner.cc
        ../classic/read_write_ar.cc
        ../classic/read_write_job.cc)
set(LIBRARY_INCLUDES "./")

# sge_spooling_flatfile_scanner is generated from flex it will produce compiler that we need to disable
set_source_files_properties(sge_spooling_flatfile_scanner.cc PROPERTIES COMPILE_FLAGS -Wno-error)

add_library(${LIBRARY_NAME}_objlib OBJECT ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME}_objlib PUBLIC ${SGE_INCLUDES} ${LIBRARY_INCLUDES})
set_property(TARGET ${LIBRARY_NAME}_objlib PROPERTY POSITION_INDEPENDENT_CODE 1)

add_library(${LIBRARY_NAME}_static STATIC $<TARGET_OBJECTS:${LIBRARY_NAME}_objlib>)
target_include_directories(${LIBRARY_NAME}_static PUBLIC ${SGE_INCLUDES} ${LIBRARY_INCLUDES})

add_library(${LIBRARY_NAME} SHARED $<TARGET_OBJECTS:${LIBRARY_NAME}_objlib>)
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES} ${LIBRARY_INCLUDES})
target_link_libraries(
      ${LIBRARY_NAME}
      PRIVATE
      spool
      sched
      sgeobj
      gdi
      cull
      comm
      uti
      commlists
)

if (INSTALL_SGE_BIN_MASTER)
   install(TARGETS spoolc DESTINATION lib/${SGE_ARCH})
endif ()

