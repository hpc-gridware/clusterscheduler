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

# The core of the typed object model calls no function of the old interface
# (N-L-1): the old functions are thin wrappers over the core, never the other
# way round. Scans every source and header under source/libs/tom for calls of
# a function named like the old interface, l followed by a capital letter, and
# fails on one that is not infrastructure the rule allows: the observation
# hooks (lObserve...) and the name space (lNm2Str). Comments and string
# literals are left out of the scan.
#
# Arguments (-D): TOM_DIR (source/libs/tom), STAMP (touched when the scan passed).

file(GLOB sources "${TOM_DIR}/*.h" "${TOM_DIR}/*.cc")
set(violations "")
foreach (source IN LISTS sources)
   file(READ "${source}" text)
   # block comments, line comments and string literals do not call anything
   string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" "" text "${text}")
   string(REGEX REPLACE "//[^\n]*" "" text "${text}")
   string(REGEX REPLACE "\"([^\"\\\\\n]|\\\\.)*\"" "\"\"" text "${text}")
   string(REGEX MATCHALL "[^A-Za-z0-9_]l[A-Z][A-Za-z0-9_]*[ \t]*\\(" calls "${text}")
   foreach (call IN LISTS calls)
      string(REGEX REPLACE "^.(l[A-Za-z0-9_]*).*" "\\1" name "${call}")
      if (name MATCHES "^lObserve" OR name STREQUAL "lNm2Str")
         continue()
      endif ()
      get_filename_component(file_name "${source}" NAME)
      list(APPEND violations "${file_name}: ${name}()")
   endforeach ()
endforeach ()
if (violations)
   list(REMOVE_DUPLICATES violations)
   string(REPLACE ";" "\n   " report "${violations}")
   message(FATAL_ERROR "the core of the typed object model calls functions of the old interface:\n   ${report}")
endif ()
file(TOUCH "${STAMP}")
