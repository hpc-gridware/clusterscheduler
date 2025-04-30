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

# source/libs/spool/dynamic
if (${WITH_SPOOL_DYNAMIC})
   set(LIBRARY_NAME spoold)
   set(LIBRARY_SOURCES sge_spooling_dynamic.cc)
   set(LIBRARY_INCLUDES "./")

   add_library(${LIBRARY_NAME}_objlib OBJECT ${LIBRARY_SOURCES})
   target_include_directories(${LIBRARY_NAME}_objlib PUBLIC ${SGE_INCLUDES} ${LIBRARY_INCLUDES})
   set_property(TARGET ${LIBRARY_NAME}_objlib PROPERTY POSITION_INDEPENDENT_CODE 1)

   add_library(${LIBRARY_NAME}_static STATIC $<TARGET_OBJECTS:${LIBRARY_NAME}_objlib>)

   add_library(${LIBRARY_NAME} SHARED $<TARGET_OBJECTS:${LIBRARY_NAME}_objlib>)
   target_link_libraries(
         ${LIBRARY_NAME}
         PRIVATE
         sgeobj
         gdi
         cull
         comm
         uti
         commlists)

   #set(CMAKE_ENABLE_EXPORTS ON)
   #set(CMAKE_C_FLAGS "-g -fno-common -dynamic")
   #set(CMAKE_EXE_LINKER_FLAGS "-g -dynamiclib -flat_namespace")

   #add_compile_options(-fno-common -dynamic)
   #add_link_options(-dynamiclib -flat_namespace)

   if (INSTALL_SGE_BIN_MASTER)
      install(TARGETS spoold DESTINATION lib/${SGE_ARCH})
   endif ()
endif ()
