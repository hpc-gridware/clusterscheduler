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

# Compile one source that must not compile (TRT-02), and fail if it does.
#
# The source names the error it has to fail with in a line "expect: <regex>",
# so that a source failing for an unrelated reason - a missing include, a typo -
# does not pass as the expected failure.
#
# Arguments (-D): COMPILER, FLAGS (the C++ flags of the build), STANDARD (the
# C++ standard), INCLUDES and DEFINES (lists from the target), SOURCE, STAMP
# (touched when the source failed as expected).

file(STRINGS "${SOURCE}" expect_line REGEX "expect: ")
if (NOT expect_line)
   message(FATAL_ERROR "${SOURCE} names no expected error")
endif ()
string(REGEX REPLACE ".*expect: " "" expect "${expect_line}")

separate_arguments(flags UNIX_COMMAND "${FLAGS}")
set(args ${flags} "-std=c++${STANDARD}" -fsyntax-only)
foreach (dir IN LISTS INCLUDES)
   list(APPEND args "-I${dir}")
endforeach ()
foreach (def IN LISTS DEFINES)
   list(APPEND args "-D${def}")
endforeach ()

execute_process(COMMAND ${COMPILER} ${args} ${SOURCE}
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE output)
if (result EQUAL 0)
   message(FATAL_ERROR "${SOURCE} compiled, but the typed object model must reject it")
endif ()
if (NOT output MATCHES "${expect}")
   message(FATAL_ERROR "${SOURCE} failed, but not with the expected error (${expect}):\n${output}")
endif ()
file(TOUCH "${STAMP}")
