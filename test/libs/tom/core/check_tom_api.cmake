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

# The internals of the typed object model stay inside it (CS-3103).
#
# Product code reaches objects through the typed object model and the raw API
# (tom/ocs_*.h). The data types are public: tom/ocs_Types.h (lList, lListElem,
# lDescr, ... and the object type definition macros) and tom/ocs_PackBuffer.h
# (sge_pack_buffer, the PACK_* codes).
#
# Two checks, both ERRORs:
#
# 1. Includes. The other headers of tom/detail/ and tom/ocs_Descr.h declare the
#    internals of the list core: including one is an error, except
#    - under source/libs/tom and source/libs/sgeobj/tom, which implement the
#      model, and
#    - in the tests (any path with a test/ directory in it), which may test the
#      core directly.
#    Headers are compared by their file name, so a relative spelling cannot slip
#    through.
#
# 2. Calls. The TOM headers need the internal headers for their inline code,
#    so the internals declared there are reachable from every translation unit
#    that includes a TOM header: tom/detail/ocs_Hash.h
#    (ocs::tom::detail::hash_*()), tom/detail/ocs_ListP.h
#    (ocs::tom::detail::slot_index(), elem_kind(), elem_set_*(),
#    list_hash_table(), ...), tom/detail/ocs_MultitypeP.h
#    (ocs::tom::detail::value(), ocs::tom::detail::bad_slot()),
#    tom/detail/ocs_WhatP.h and tom/ocs_Descr.h (lCountPresent(),
#    ocs::tom::descr_*()). Outside the same exceptions no file may call one of
#    them. The names are taken from the declarations in these headers: a
#    declaration of a global name starts in column 0, one in a namespace block
#    (`namespace ocs::tom {` or `namespace ocs::tom::detail {` in column 0) is
#    indented by one level, three blanks (documentation and bodies are indented
#    deeper). Left out are descr_make*(), which LISTDEF expands to in every
#    object type definition, and the names the public raw API tom/ocs_Raw.h
#    declares as well (release_descr(), ...). A call of a global name is the
#    name as a whole word, not after `.`, `->` or `::`, followed by `(`; a call
#    of a name of a namespace is the name qualified with `tom::` or `detail::`
#    (ocs::tom::descr_ref(), ocs::tom::detail::hash_key(), ...), followed by
#    `(`; an unqualified call inside a namespace block of ocs::tom is not
#    found. Comments, string and character literals are removed first. A plain
#    substring search finds the candidate files; only those are cleaned and
#    scanned with a regular expression.
#
# Arguments (-D): SOURCE_DIR (the clusterscheduler checkout), EXTENSIONS_DIR
# (the extensions checkout, optional), STAMP (optional, touched when the scan
# passed). Run by hand with
#   cmake -DSOURCE_DIR=<clusterscheduler> -DEXTENSIONS_DIR=<gcs-extensions> -P check_tom_api.cmake

# a script run with -P sets no policy version, so if (... IN_LIST ...) needs the policy set here
cmake_policy(SET CMP0057 NEW)

# ---- the headers product code must not include
set(forbidden "")
file(GLOB detail_headers RELATIVE "${SOURCE_DIR}/source/libs" "${SOURCE_DIR}/source/libs/tom/detail/*.h")
foreach (header IN LISTS detail_headers)
   list(APPEND forbidden "${header}")
endforeach ()
list(APPEND forbidden "tom/ocs_Descr.h")
# headers are compared by their file name, so "ocs_Hash.h" or "./ocs_Hash.h" reach the same header as
# "tom/detail/ocs_Hash.h"
set(forbidden_names "")
foreach (header IN LISTS forbidden)
   get_filename_component(name "${header}" NAME)
   list(APPEND forbidden_names "${name}")
endforeach ()
list(REMOVE_DUPLICATES forbidden_names)
# one pattern for all of them
set(include_alternation "${forbidden_names}")
string(REPLACE "." "\\." include_alternation "${include_alternation}")
string(REPLACE ";" "|" include_alternation "${include_alternation}")
set(include_pattern "#[ \t]*include[ \t]*[<\"]([^>\"\n]*/)?(${include_alternation})[>\"]")

# ---- the internals product code must not call
# collect_declarations(<header> <global names var> <member names var>): the names a header declares, global ones
# (a declaration starts in column 0) and members of a namespace block opened in column 0 (a declaration is indented
# by three blanks; documentation and bodies are indented deeper). The name is the first identifier followed by `(`,
# also where the return type is on the line before.
function(collect_declarations header global_var member_var)
   set(global_names "")
   set(member_names "")
   set(depth 0)
   file(STRINGS "${header}" lines)
   foreach (line IN LISTS lines)
      if (line MATCHES "^namespace [A-Za-z_][A-Za-z0-9_:]*[ \t]*[{]")
         math(EXPR depth "${depth} + 1")
         continue ()
      endif ()
      if (depth GREATER 0 AND line MATCHES "^[}]")
         math(EXPR depth "${depth} - 1")
         continue ()
      endif ()
      # an attribute such as [[nodiscard]] in front of a declaration
      string(REGEX REPLACE "^([ \t]*)\\[\\[[^]]*\\]\\][ \t]*" "\\1" line "${line}")
      if (depth EQUAL 0)
         set(indent "")
      else ()
         set(indent "   ")
      endif ()
      if (NOT line MATCHES "^${indent}[A-Za-z_][^;{}=]*\\(")
         continue ()
      endif ()
      string(STRIP "${line}" decl)
      if (decl MATCHES "^(typedef|using|namespace|class|struct|enum|template|static_assert|return)[^A-Za-z0-9_]")
         continue ()
      endif ()
      if (decl MATCHES "([A-Za-z_][A-Za-z0-9_]*)[ \t]*\\(")
         if (depth EQUAL 0)
            list(APPEND global_names "${CMAKE_MATCH_1}")
         else ()
            list(APPEND member_names "${CMAKE_MATCH_1}")
         endif ()
      endif ()
   endforeach ()
   set(${global_var} "${global_names}" PARENT_SCOPE)
   set(${member_var} "${member_names}" PARENT_SCOPE)
endfunction()

set(internal_headers detail/ocs_Hash.h detail/ocs_ListP.h detail/ocs_MultitypeP.h detail/ocs_WhatP.h ocs_Descr.h)
set(internal_names "")
set(internal_member_names "")
foreach (header IN LISTS internal_headers)
   collect_declarations("${SOURCE_DIR}/source/libs/tom/${header}" global_names member_names)
   list(APPEND internal_names ${global_names})
   list(APPEND internal_member_names ${member_names})
endforeach ()
# descr_make*() is what LISTDEF expands to in the object type definitions
list(FILTER internal_member_names EXCLUDE REGEX "^descr_make")
# what the public raw API declares as well is public
collect_declarations("${SOURCE_DIR}/source/libs/tom/ocs_Raw.h" public_global_names public_member_names)
foreach (name IN LISTS public_member_names)
   list(REMOVE_ITEM internal_member_names "${name}")
endforeach ()
list(REMOVE_DUPLICATES internal_names)
list(REMOVE_DUPLICATES internal_member_names)
# The candidate search: the names up to their second `_` (elem_set_, hash_new_, ...), or whole, a member name
# with the `::` in front of it, searched with string(FIND), which costs far less than a regular expression over
# every file.
set(global_stems "")
foreach (name IN LISTS internal_names)
   if (name MATCHES "^([A-Za-z0-9]*_[A-Za-z0-9]*_)")
      list(APPEND global_stems "${CMAKE_MATCH_1}")
   else ()
      list(APPEND global_stems "${name}")
   endif ()
endforeach ()
list(REMOVE_DUPLICATES global_stems)
set(internal_stems ${global_stems})
foreach (name IN LISTS internal_member_names)
   if (name MATCHES "^([A-Za-z0-9]*_[A-Za-z0-9]*_)")
      list(APPEND internal_stems "::${CMAKE_MATCH_1}")
   else ()
      list(APPEND internal_stems "::${name}")
   endif ()
endforeach ()
list(REMOVE_DUPLICATES internal_stems)
string(REPLACE ";" "|" stems_alternation "${global_stems}")
set(call_pattern "[^A-Za-z0-9_.>:](${stems_alternation})[A-Za-z0-9_]*[ \t]*\\(")
string(REPLACE ";" "|" members_alternation "${internal_member_names}")
set(member_call_pattern "(tom|detail)::(${members_alternation})[ \t]*\\(")
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
      if (rel MATCHES "^source/libs/(tom|tom|sgeobj/tom)/")
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
            if (name IN_LIST forbidden_names)
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
      if (internal_member_names)
         string(REGEX MATCHALL "${member_call_pattern}" calls "${content}")
         foreach (call IN LISTS calls)
            if (call MATCHES "::([A-Za-z_][A-Za-z0-9_]*)[ \t]*\\($" AND CMAKE_MATCH_1 IN_LIST internal_member_names)
               list(APPEND called "${CMAKE_MATCH_1}()")
            endif ()
         endforeach ()
      endif ()
      if (called)
         list(REMOVE_DUPLICATES called)
         string(REPLACE ";" ", " called "${called}")
         list(APPEND violations "${source}: calls ${called} (internal of the list library)")
      endif ()
   endforeach ()
endforeach ()

if (violations)
   string(REPLACE ";" "\n   " report "${violations}")
   message(FATAL_ERROR "internals of the typed object model are used outside libs/tom, libs/sgeobj/tom and the "
                       "tests (see check_tom_api.cmake):\n   ${report}")
endif ()
if (STAMP)
   file(TOUCH "${STAMP}")
endif ()
