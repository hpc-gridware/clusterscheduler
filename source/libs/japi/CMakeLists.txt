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

# source/libs/japi
set(LIBRARY_NAME japi)
set(LIBRARY_SOURCES japi.cc)
set(LIBRARY_INCLUDES "./")

if (WITH_JNI)
   set(JNI_CODE ../jdrmaa/com_sun_grid_drmaa_SessionImpl.c)
   set(JNI_INCLUDE $ENV{JAVA_HOME}/include $ENV{JAVA_HOME}/include/${JNI_ARCH})
endif()

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES} ${JNI_INCLUDE}
      ${LIBRARY_INCLUDES})

set(DRMAA_LIB_NAME drmaa)
set(DRMAA_LIB_SOURCES drmaa.cc
        ../../common/parse_job_cull.cc
        ../../common/parse_qsub.cc
        ${JNI_CODE}
)
set(DRMAA_LIB_INCLUDES "./")

add_library(${DRMAA_LIB_NAME} SHARED ${DRMAA_LIB_SOURCES})
target_include_directories(${DRMAA_LIB_NAME} PUBLIC ${SGE_INCLUDES}
      ${DRMAA_LIB_INCLUDES})
target_link_libraries(${DRMAA_LIB_NAME} PRIVATE
      clientscommon
      japi
      evc
      gdi
      sgeobj
      cull
      comm
      commlists
      uti
      ${SGE_LIBS_SHARED}
)

# test binaries
add_executable(test_drmaa ../../../test/libs/drmaa/test_drmaa.cc)
target_include_directories(test_drmaa PRIVATE "./")
target_link_libraries(test_drmaa PRIVATE drmaa ${SGE_LIBS})


if (INSTALL_SGE_COMMON)
   install(FILES drmaa.h DESTINATION include)

   install(
         FILES
         howto/example.c
         howto/howto1_1.c
         howto/howto1.c
         howto/howto2_1.c
         howto/howto2.c
         howto/howto3_1.c
         howto/howto3_2.c
         howto/howto3.c
         howto/howto4.c
         howto/howto5.c
         howto/howto6.c
         DESTINATION examples/drmaa)
endif ()

if (INSTALL_SGE_BIN_CLIENT)
   install(TARGETS drmaa DESTINATION lib/${SGE_ARCH})
endif ()

