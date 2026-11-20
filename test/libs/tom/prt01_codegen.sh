#!/bin/sh
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

# PRT-01 (CS-3044): compile prt01_probe.cc to assembly with the Release optimisation of the line build
# and print the code of each probe function.
#
# usage: prt01_codegen.sh <build dir> <c++ compiler> [<output .s>]
#
# The include paths and defines come from the build directory's sgeobj target, so the probe sees the
# generated headers of that build. -fno-lto keeps machine code in the output (LTO would emit IR).
# Read the result against PRT-01: on the path without a throw no call, the loads of the descriptor, the
# slot offset, the value array and the value, a compare and branch for an empty handle and one for
# presence, and on an untyped pointer one more for the type.

set -e
if [ $# -lt 2 ]; then
   echo "usage: $0 <build dir> <c++ compiler> [<output .s>]" >&2
   exit 1
fi
build=$1
cxx=$2
out=${3:-/tmp/prt01_probe.$$.s}
here=$(cd "$(dirname "$0")" && pwd)
flags=$build/source/libs/sgeobj/CMakeFiles/sgeobj.dir/flags.make
defines=$(sed -n 's/^CXX_DEFINES = //p' "$flags")
includes=$(sed -n 's/^CXX_INCLUDES = //p' "$flags")

eval "\"$cxx\" -O3 -DNDEBUG -std=c++23 -fno-lto -fno-asynchronous-unwind-tables $defines $includes \
   -S -o \"$out\" \"$here/prt01_probe.cc\""
echo "# $("$cxx" --version | head -1)"

for fn in prt01_typed_number prt01_typed_string prt01_untyped_number prt01_untyped_string; do
   label=$(grep -m1 -oE "^_Z[0-9]*${fn}[A-Za-z0-9_]*:" "$out")
   echo "=== $fn"
   # the body up to the end of the function, without directives, comments and local labels
   awk -v l="$label" 'index($0, l) == 1 { f = 1; next }
                      f && (/^\.Lfunc_end/ || /^\t\.cfi_endproc/ || /^\t\.size/) { exit }
                      f' "$out" | grep -vE '^\s*\.|^\s*#|^\.?L[A-Za-z_0-9]*:' | sed 's/#.*//' | grep -v '^\s*$' | c++filt
done
