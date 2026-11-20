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
# (tom/ocs_*.h). Of the old library it may include only cull/cull.h, the data
# types: tom/cull_types.h (lList, lListElem, lDescr, ... and the object type
# definition macros) and tom/ocs_PackBuffer.h (sge_pack_buffer, the PACK_*
# codes); tom/cull_lerrno.h (the error codes the cull error state is set with)
# may be included directly.
#
# Two checks, both ERRORs:
#
# 1. Includes. Every other header of source/libs/cull (cull/cull_old_api.h and
#    the headers it collects, cull/pack.h), tom/pack.h (the old pack buffer
#    primitives), the other cull_*.h headers of source/libs/tom and
#    tom/ocs_cull_descr.h declare the old function interface or its internals:
#    including one is an error, except
#    - under source/libs/cull, source/libs/tom and source/libs/sgeobj/tom, which
#      implement the old interface or wrap it, and
#    - in the tests (any path with a test/ directory in it). Tests that still
#      build their data with the old interface include cull/cull_test_api.h,
#      which no file outside the tests may include.
#
# 2. Calls. The old function interface is not declared in a product translation
#    unit any more, the compiler refuses its names. The TOM headers still need
#    internal headers of the list library for their inline code, so the
#    internals declared there stay reachable until a later CS-3103 step moves
#    them behind detail headers of tom: tom/cull_hash.h (cull_hash_*,
#    ocs_cull_hash_*), tom/cull_listP.h (ocs_cull_slot(), elem_kind(),
#    elem_set_*(), list_hash_table(), ...), tom/cull_multitypeP.h
#    (ocs_cull_value(), ocs_cull_bad_slot()), tom/cull_whatP.h and
#    tom/ocs_cull_descr.h (lCountPresent(), ocs_cull_descr_*()). Outside the
#    same exceptions no file may call one of them. The names are taken from the
#    declarations in these headers (a declaration starts in column 0), except
#    ocs_cull_descr_make*(), which LISTDEF expands to in every object type
#    definition. A call is a name as a whole word, not after `.`, `->` or `::`,
#    followed by `(`; comments, string and character literals are removed
#    first. A plain substring search finds the candidate files; only those are
#    cleaned and scanned with a regular expression.
#
# Arguments (-D): SOURCE_DIR (the clusterscheduler checkout), EXTENSIONS_DIR
# (the extensions checkout, optional), STAMP (optional, touched when the scan
# passed). Run by hand with
#   cmake -DSOURCE_DIR=<clusterscheduler> -DEXTENSIONS_DIR=<gcs-extensions> -P check_cull_api.cmake

# a script run with -P sets no policy version, so if (... IN_LIST ...) needs the policy set here
cmake_policy(SET CMP0057 NEW)

# ---- the headers product code must not include
set(forbidden "")
file(GLOB cull_headers RELATIVE "${SOURCE_DIR}/source/libs" "${SOURCE_DIR}/source/libs/cull/*.h")
foreach (header IN LISTS cull_headers)
   if (NOT header STREQUAL "cull/cull.h")
      list(APPEND forbidden "${header}")
   endif ()
endforeach ()
file(GLOB tom_headers RELATIVE "${SOURCE_DIR}/source/libs" "${SOURCE_DIR}/source/libs/tom/cull_*.h")
foreach (header IN LISTS tom_headers)
   if (NOT header MATCHES "^tom/cull_(types|lerrno)\\.h$")
      list(APPEND forbidden "${header}")
   endif ()
endforeach ()
list(APPEND forbidden "tom/ocs_cull_descr.h" "tom/pack.h")
# headers are compared by their file name: libs/cull and libs/tom export their own directories as include
# paths, so "cull_list.h" or "./cull_list.h" reach the same header as "cull/cull_list.h"
set(forbidden_names "")
foreach (header IN LISTS forbidden)
   get_filename_component(name "${header}" NAME)
   list(APPEND forbidden_names "${name}")
endforeach ()
# the public names win where an old and a public header share a file name (cull/cull_lerrno.h forwards to
# tom/cull_lerrno.h)
list(REMOVE_ITEM forbidden_names "cull.h" "cull_types.h" "cull_lerrno.h")
list(REMOVE_DUPLICATES forbidden_names)
# one pattern for all of them, cull_test_api.h included
set(include_alternation "${forbidden_names};cull_test_api.h")
string(REPLACE "." "\\." include_alternation "${include_alternation}")
string(REPLACE ";" "|" include_alternation "${include_alternation}")
set(include_pattern "#[ \t]*include[ \t]*[<\"]([^>\"\n]*/)?(${include_alternation})[>\"]")

# ---- the internals product code must not call
set(internal_headers cull_hash.h cull_listP.h cull_multitypeP.h cull_whatP.h ocs_cull_descr.h)
set(internal_names "")
foreach (header IN LISTS internal_headers)
   # a declaration starts in column 0 (documentation and bodies are indented); the name is the first identifier
   # followed by `(`, also where the return type is on the line before
   file(STRINGS "${SOURCE_DIR}/source/libs/tom/${header}" decls REGEX "^[A-Za-z_][^;{}=]*\\(")
   foreach (decl IN LISTS decls)
      if (decl MATCHES "^(typedef|using|namespace|class|struct|enum|template|static_assert|return)[^A-Za-z0-9_]")
         continue ()
      endif ()
      if (decl MATCHES "([A-Za-z_][A-Za-z0-9_]*)[ \t]*\\(")
         list(APPEND internal_names "${CMAKE_MATCH_1}")
      endif ()
   endforeach ()
endforeach ()
# ocs_cull_descr_make*() is what LISTDEF expands to in the object type definitions
list(FILTER internal_names EXCLUDE REGEX "^ocs_cull_descr_make")
list(REMOVE_DUPLICATES internal_names)
# The candidate search: the names up to their second `_` (cull_hash_, ocs_cull_, elem_set_, ...), or whole,
# searched with string(FIND), which costs far less than a regular expression over every file.
set(internal_stems "")
foreach (name IN LISTS internal_names)
   if (name MATCHES "^([A-Za-z0-9]*_[A-Za-z0-9]*_)")
      list(APPEND internal_stems "${CMAKE_MATCH_1}")
   else ()
      list(APPEND internal_stems "${name}")
   endif ()
endforeach ()
list(REMOVE_DUPLICATES internal_stems)
string(REPLACE ";" "|" stems_alternation "${internal_stems}")
set(call_pattern "[^A-Za-z0-9_.>:](${stems_alternation})[A-Za-z0-9_]*[ \t]*\\(")
# a string literal, a character literal, a line comment or a block comment; the leftmost match wins, so a
# comment start inside a literal stays part of the literal and a quote inside a comment part of the comment
string(CONCAT noise_pattern "\"[^\"\\\\\n]*(\\\\.[^\"\\\\\n]*)*\"|'[^'\\\\\n]*(\\\\.[^'\\\\\n]*)*'"
       "|//[^\n]*|/\\*[^*]*\\*+([^/*][^*]*\\*+)*/")

set(roots "${SOURCE_DIR}/source" "${SOURCE_DIR}/test")
if (EXTENSIONS_DIR AND IS_DIRECTORY "${EXTENSIONS_DIR}")
   list(APPEND roots "${EXTENSIONS_DIR}/source" "${EXTENSIONS_DIR}/test")
endif ()

set(violations "")
foreach (root IN LISTS roots)
   if (NOT IS_DIRECTORY "${root}")
      continue ()
   endif ()
   file(GLOB_RECURSE sources "${root}/*.h" "${root}/*.hh" "${root}/*.c" "${root}/*.cc" "${root}/*.cpp")
   foreach (source IN LISTS sources)
      file(RELATIVE_PATH rel "${root}/.." "${source}")
      if (rel MATCHES "(^|/)test/")
         continue ()
      endif ()
      if (rel MATCHES "^source/libs/(cull|tom|sgeobj/tom)/")
         continue ()
      endif ()
      # the text starts with a line break, so that a call has a character before the name
      file(READ "${source}" content)
      string(PREPEND content "\n")
      # One pattern that starts with `#` finds a forbidden include fast; the anchored scan of every #include
      # line runs only then, and confirms it (an #include in a comment could fake one).
      string(REGEX MATCH "${include_pattern}" suspect "${content}")
      if (suspect)
         string(REGEX MATCHALL "\n[ \t]*#[ \t]*include[ \t]*[<\"][^>\"\n]*" includes "${content}")
         foreach (include IN LISTS includes)
            string(REGEX REPLACE "^\n[ \t]*#[ \t]*include[ \t]*[<\"]" "" header "${include}")
            get_filename_component(name "${header}" NAME)
            if (name STREQUAL "cull_test_api.h")
               list(APPEND violations "${source}: #include \"${header}\" (tests only)")
            elseif (name IN_LIST forbidden_names)
               list(APPEND violations "${source}: #include \"${header}\" (not public)")
            endif ()
         endforeach ()
      endif ()
      set(candidate FALSE)
      foreach (stem IN LISTS internal_stems)
         string(FIND "${content}" "${stem}" position)
         if (position GREATER -1)
            set(candidate TRUE)
            break ()
         endif ()
      endforeach ()
      if (NOT candidate)
         continue ()
      endif ()
      # a candidate may be in a comment or a literal: clean the text and look again
      string(REGEX REPLACE "${noise_pattern}" " " content "${content}")
      string(REGEX MATCHALL "${call_pattern}" calls "${content}")
      set(called "")
      foreach (call IN LISTS calls)
         # a leading `;` of a match splits it into an empty and a named element
         if (call MATCHES "([A-Za-z_][A-Za-z0-9_]*)[ \t]*\\($" AND CMAKE_MATCH_1 IN_LIST internal_names)
            list(APPEND called "${CMAKE_MATCH_1}()")
         endif ()
      endforeach ()
      if (called)
         list(REMOVE_DUPLICATES called)
         string(REPLACE ";" ", " called "${called}")
         list(APPEND violations "${source}: calls ${called} (internal of the list library)")
      endif ()
   endforeach ()
endforeach ()

if (violations)
   string(REPLACE ";" "\n   " report "${violations}")
   message(FATAL_ERROR "the old interface of the list library or its internals are used outside libs/cull, "
                       "libs/tom, libs/sgeobj/tom and the tests (see check_cull_api.cmake):\n   ${report}")
endif ()
if (STAMP)
   file(TOUCH "${STAMP}")
endif ()
