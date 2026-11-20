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
# (N-L-1) and includes nothing but its own headers, those of libs/uti and
# system headers (N-L-5): the old functions are thin wrappers over the core,
# never the other way round, and the storage layer lives in the core.
#
# Scans every source and header under source/libs/tom
# - for includes of a header outside tom/ and uti/;
# - for calls of a function named like the old interface, l followed by a
#   capital letter, that is not defined in source/libs/tom itself. The
#   functions defined there (lCountPresent(), ...) are the
#   infrastructure that moved into the core with the storage layer; the
#   types it defines (lDescr, lList, ...) are no calls either. The list
#   walking macros (for_each_ep() and its siblings) count as calls, since
#   they expand to lFirst() and lNext().
# Comments and string literals are left out of the scan for calls.
#
# Arguments (-D): TOM_DIR (source/libs/tom), STAMP (touched when the scan passed).

# a script run with -P sets no policy version, so if (... IN_LIST ...) needs the policy set here
cmake_policy(SET CMP0057 NEW)

file(GLOB sources "${TOM_DIR}/*.h" "${TOM_DIR}/*.cc")
set(violations "")
set(texts "")

# first pass: strip comments, check includes, collect the functions defined in the core
set(defined "")
foreach (source IN LISTS sources)
   get_filename_component(file_name "${source}" NAME)
   file(READ "${source}" text)
   string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" "" text "${text}")
   string(REGEX REPLACE "//[^\n]*" "" text "${text}")
   string(REGEX MATCHALL "#[ \t]*include[ \t]*\"[^\"]*\"" includes "${text}")
   foreach (include IN LISTS includes)
      string(REGEX REPLACE ".*\"([^\"]*)\"" "\\1" header "${include}")
      if (NOT header MATCHES "^(tom|uti)/")
         list(APPEND violations "${file_name}: #include \"${header}\"")
      endif ()
   endforeach ()
   string(REGEX REPLACE "\"([^\"\\\\\n]|\\\\.)*\"" "\"\"" text "${text}")
   set(text_${file_name} "${text}")
   list(APPEND texts "${file_name}")
   # a type of the storage layer followed by a parenthesis is not a call
   string(REGEX MATCHALL "typedef[^;]*" typedefs "${text}")
   foreach (typedef IN LISTS typedefs)
      if (typedef MATCHES "[ *](l[A-Z][A-Za-z0-9_]*)[ \t\n]*$")
         list(APPEND defined "${CMAKE_MATCH_1}")
      endif ()
   endforeach ()
   # a template definition in a header, such as the old walking names overloaded for the handles, is no call
   string(REGEX MATCHALL "template[ \t]*<[^>]*>[^;{}()]*[ *&]l[A-Z][A-Za-z0-9_]*[ \t]*\\(" template_heads "${text}")
   foreach (head IN LISTS template_heads)
      string(REGEX REPLACE ".*[ *&](l[A-Z][A-Za-z0-9_]*)[ \t]*\\(.*" "\\1" name "${head}")
      list(APPEND defined "${name}")
   endforeach ()
   if (file_name MATCHES "\\.cc$")
      # a definition starts in the first column and reaches its body before a semicolon
      string(REGEX MATCHALL "\n[A-Za-z_][^\n;{}()]*[ *&]l[A-Z][A-Za-z0-9_]*[ \t]*\\([^;{]*{" heads "${text}")
      foreach (head IN LISTS heads)
         string(REGEX REPLACE ".*[ *&](l[A-Z][A-Za-z0-9_]*)[ \t]*\\(.*" "\\1" name "${head}")
         list(APPEND defined "${name}")
      endforeach ()
   endif ()
endforeach ()

# second pass: every call of an l... function must be one the core defines
foreach (file_name IN LISTS texts)
   string(REGEX MATCHALL "[^A-Za-z0-9_]l[A-Z][A-Za-z0-9_]*[ \t]*\\(" calls "${text_${file_name}}")
   foreach (call IN LISTS calls)
      string(REGEX REPLACE "^.(l[A-Za-z0-9_]*).*" "\\1" name "${call}")
      if (NOT name IN_LIST defined)
         list(APPEND violations "${file_name}: ${name}()")
      endif ()
   endforeach ()
   # the list walking macros of the old interface expand to lFirst(), lNext(), ...
   string(REGEX MATCHALL "[^A-Za-z0-9_]for_each_[a-z_]+[ \t]*\\(" walks "${text_${file_name}}")
   foreach (walk IN LISTS walks)
      string(REGEX REPLACE "^.(for_each_[a-z_]+).*" "\\1" name "${walk}")
      list(APPEND violations "${file_name}: ${name}()")
   endforeach ()
endforeach ()

if (violations)
   list(REMOVE_DUPLICATES violations)
   string(REPLACE ";" "\n   " report "${violations}")
   message(FATAL_ERROR "the core of the typed object model depends on the old interface:\n   ${report}")
endif ()
file(TOUCH "${STAMP}")
