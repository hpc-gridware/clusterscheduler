#___INFO__MARK_BEGIN_NEW__
###########################################################################
#
#  Copyright 2026 HPC-Gridware GmbH
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

# The old function interface of the list library is not public (CS-3103, M6).
#
# Product code reaches objects through the typed object model and the raw API
# (tom/ocs_*.h). Of the old library it may include
# - cull/cull.h, the data types: tom/cull_types.h (lList, lListElem, lDescr, ...
#   and the object type definition macros) and tom/ocs_PackBuffer.h
#   (sge_pack_buffer, the PACK_* codes); tom/cull_lerrno.h (the error codes the
#   cull error state is set with) may be included directly;
# - for now, cull/cull_transitional.h (list walking, lGetElemDescr(), ... and the
#   pack buffer primitives), also as cull/pack.h or tom/pack.h. The next CS-3103
#   step removes these users; until then each one is listed as a WARNING.
# Every other header of source/libs/cull (cull/cull_old_api.h and the headers it
# collects), the other cull_*.h headers of source/libs/tom and
# tom/ocs_cull_descr.h declare the old function interface or its internals:
# including one is an ERROR, except
# - under source/libs/cull, source/libs/tom and source/libs/sgeobj/tom, which
#   implement the old interface or wrap it, and
# - in the tests (any path with a test/ directory in it). Tests that still
#   build their data with the old interface include cull/cull_test_api.h,
#   which no file outside the tests may include (an ERROR as well).
#
# Scans the #include lines of every source and header under the roots.
#
# Arguments (-D): SOURCE_DIR (the clusterscheduler checkout), EXTENSIONS_DIR
# (the extensions checkout, optional), STAMP (optional, touched when the scan
# passed). Run by hand with
#   cmake -DSOURCE_DIR=<clusterscheduler> -DEXTENSIONS_DIR=<gcs-extensions> -P check_cull_api.cmake

# a script run with -P sets no policy version, so if (... IN_LIST ...) needs the policy set here
cmake_policy(SET CMP0057 NEW)

# the headers product code must not include
set(forbidden "")
file(GLOB cull_headers RELATIVE "${SOURCE_DIR}/source/libs" "${SOURCE_DIR}/source/libs/cull/*.h")
foreach (header IN LISTS cull_headers)
   if (NOT header MATCHES "^cull/(cull|cull_transitional|pack)\\.h$")
      list(APPEND forbidden "${header}")
   endif ()
endforeach ()
file(GLOB tom_headers RELATIVE "${SOURCE_DIR}/source/libs" "${SOURCE_DIR}/source/libs/tom/cull_*.h")
foreach (header IN LISTS tom_headers)
   if (NOT header MATCHES "^tom/cull_(types|lerrno)\\.h$")
      list(APPEND forbidden "${header}")
   endif ()
endforeach ()
list(APPEND forbidden "tom/ocs_cull_descr.h")
# the headers product code may still include until the next CS-3103 step
set(transitional "cull/cull_transitional.h" "cull/pack.h" "tom/pack.h")
# headers are compared by their file name: libs/cull and libs/tom export their own directories as include
# paths, so "cull_list.h" or "./cull_list.h" reach the same header as "cull/cull_list.h"
set(forbidden_names "")
foreach (header IN LISTS forbidden)
   get_filename_component(name "${header}" NAME)
   list(APPEND forbidden_names "${name}")
endforeach ()
set(transitional_names "cull_transitional.h" "pack.h")
# the public names win where an old and a public header share a file name (cull/cull_lerrno.h forwards to
# tom/cull_lerrno.h)
list(REMOVE_ITEM forbidden_names "cull.h" "cull_types.h" "cull_lerrno.h" "cull_transitional.h" "pack.h")

set(roots "${SOURCE_DIR}/source" "${SOURCE_DIR}/test")
if (EXTENSIONS_DIR AND IS_DIRECTORY "${EXTENSIONS_DIR}")
   list(APPEND roots "${EXTENSIONS_DIR}/source" "${EXTENSIONS_DIR}/test")
endif ()

set(violations "")
set(transitional_users "")
foreach (root IN LISTS roots)
   if (NOT IS_DIRECTORY "${root}")
      continue ()
   endif ()
   file(GLOB_RECURSE sources "${root}/*.h" "${root}/*.hh" "${root}/*.c" "${root}/*.cc" "${root}/*.cpp")
   foreach (source IN LISTS sources)
      file(RELATIVE_PATH rel "${root}/.." "${source}")
      set(is_test FALSE)
      if (rel MATCHES "(^|/)test/")
         set(is_test TRUE)
      endif ()
      set(is_impl FALSE)
      if (rel MATCHES "^source/libs/(cull|tom|sgeobj/tom)/")
         set(is_impl TRUE)
      endif ()
      if (is_test)
         continue ()
      endif ()
      file(STRINGS "${source}" includes REGEX "^[ \t]*#[ \t]*include[ \t]*[<\"]")
      foreach (include IN LISTS includes)
         string(REGEX REPLACE "^[ \t]*#[ \t]*include[ \t]*[<\"]([^>\"]*)[>\"].*" "\\1" header "${include}")
         get_filename_component(name "${header}" NAME)
         if (name STREQUAL "cull_test_api.h")
            list(APPEND violations "${source}: #include \"${header}\" (tests only)")
         elseif (NOT is_impl AND name IN_LIST forbidden_names)
            list(APPEND violations "${source}: #include \"${header}\" (not public)")
         elseif (NOT is_impl AND name IN_LIST transitional_names)
            list(APPEND transitional_users "${source}: #include \"${header}\"")
         endif ()
      endforeach ()
   endforeach ()
endforeach ()

if (transitional_users)
   list(LENGTH transitional_users count)
   string(REPLACE ";" "\n   " report "${transitional_users}")
   message(WARNING "${count} files outside libs/cull, libs/tom, libs/sgeobj/tom and the tests still use the "
                   "transitional part of the old list library (see check_cull_api.cmake):\n   ${report}")
endif ()
if (violations)
   string(REPLACE ";" "\n   " report "${violations}")
   message(FATAL_ERROR "the old interface of the list library is included outside libs/cull, libs/tom, "
                       "libs/sgeobj/tom and the tests (see check_cull_api.cmake):\n   ${report}")
endif ()
if (STAMP)
   file(TOUCH "${STAMP}")
endif ()
