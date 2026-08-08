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

# clusterscheduler/doc/doxygen

# check if Doxygen is installed
find_package(Doxygen)
if (DOXYGEN_FOUND)
   message("Doxygen build started")

   # set input and output files
   set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
   set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

   # copy the Doxygen configuration file to the build location
   configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

   # add the target to the ALL target
   add_custom_target(doc_doxygen
         COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../..
         COMMENT "Generating API documentation with Doxygen"
         VERBATIM)

   # copy the html directory as part of the installation but only if enabled and when in Debug mode
   if (INSTALL_SGE_SRCDOC AND CMAKE_BUILD_TYPE MATCHES "^Debug")
      install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc_doxygen/html DESTINATION doc/doxygen)
   endif ()

   # ------------------------------------------------------------------------
   # Documentation gate for modules that have been converted to doxygen.
   #
   # Doxyfile.strict parses only the modules listed below and turns every
   # documentation defect in them into a build error, so a converted module
   # cannot silently regress. Modules that have not been converted yet are not
   # listed and are therefore not checked.
   #
   # To register a module: convert it with the runbook, then add one line here.
   # Paths are relative to the clusterscheduler directory.
   #
   # See doc/doxygen/Readme-dxm.md
   # ------------------------------------------------------------------------
   option(DOXYGEN_STRICT "Fail the build on documentation defects in converted modules" ON)

   set(DXM_FINISHED_MODULES
         source/libs/evc
         source/libs/uti
         source/libs/cull
         source/libs/gdi
         source/libs/sgeobj
         source/libs/evm
         source/libs/mir
   )

   if (DXM_FINISHED_MODULES)
      # dxm-check.sh parses the whole tree - needed so cross module references
      # resolve - and then fails only on warnings inside the listed modules.
      if (DOXYGEN_STRICT)
         add_custom_target(doc_doxygen_strict ALL
               COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/dxm-check.sh ${DXM_FINISHED_MODULES}
               WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..
               COMMENT "Checking documentation of converted modules"
               VERBATIM)
      else ()
         add_custom_target(doc_doxygen_strict
               COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/dxm-check.sh ${DXM_FINISHED_MODULES}
               WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..
               COMMENT "Checking documentation of converted modules"
               VERBATIM)
      endif ()
   else ()
      message(STATUS "No converted modules registered yet, documentation gate is inactive")
   endif ()
else ()
   message("Doxygen need to be installed to generate the doxygen documentation")
endif ()
