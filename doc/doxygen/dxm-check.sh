#!/bin/bash
#
# Run the strict documentation check against one or more directories.
#
# This is Gate C of the module conversion runbook: it answers "is this module
# finished?" without needing a cmake build tree, so it can be run in a tight
# loop while converting a module.
#
# Doxygen parses the whole source tree - that is required, otherwise every cross
# reference into a neighbouring module fails to resolve - and this script then
# filters the warnings down to the directories given on the command line. Only
# those decide the exit code.
#
# Usage: dxm-check.sh <dir> [dir ...]
#
#   cd clusterscheduler
#   doc/doxygen/dxm-check.sh source/libs/evc
#
# Exit: 0  no documentation defects
#       1  defects found (they are printed)
#       2  usage or environment problem
#
# See vault: 02 Work Projects/22 Doxygen Comment Migration/04 Module runbook.md

set -u

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)

if [ $# -eq 0 ]; then
   echo "usage: $(basename "$0") <dir> [dir ...]" >&2
   exit 2
fi

if ! command -v doxygen > /dev/null 2>&1; then
   echo "dxm-check: doxygen is not installed" >&2
   exit 2
fi

ABS_DIRS=""
for d in "$@"; do
   if [ ! -d "$d" ]; then
      echo "dxm-check: not a directory: $d" >&2
      exit 2
   fi
   ABS_DIRS="$ABS_DIRS $(cd "$d" && pwd)"
done

# The main Doxyfile's INPUT is relative to the workspace root (the directory
# holding clusterscheduler/ and gcs-extensions/), and the doc_doxygen target
# runs from there. Doing the same here is not cosmetic: run from anywhere else,
# doxygen finds no input, reports no warnings, and the gate would pass silently.
WORKSPACE_ROOT=$(cd "$SCRIPT_DIR/../../.." && pwd)
cd "$WORKSPACE_ROOT" || exit 2

TMPDIR_WORK=$(mktemp -d) || exit 2
trap 'rm -rf "$TMPDIR_WORK"' EXIT

# Doxyfile.strict pulls the main Doxyfile in via @INCLUDE. In a cmake build both
# have been through configure_file; here they have not, so the @INCLUDE is
# pointed at the source Doxyfile instead. The only @VAR@ left unexpanded in it
# is OUTPUT_DIRECTORY, which is overridden below anyway.
{
   sed -e "s|^@INCLUDE .*|@INCLUDE = $SCRIPT_DIR/Doxyfile|" \
       -e "s|^OUTPUT_DIRECTORY .*|OUTPUT_DIRECTORY = $TMPDIR_WORK/out/|" \
       "$SCRIPT_DIR/Doxyfile.strict"
   echo "WARN_LOGFILE = $TMPDIR_WORK/warnings.log"
   # WARN_AS_ERROR would make doxygen exit non-zero for warnings anywhere in the
   # tree, including the modules that have not been converted yet. The exit code
   # is decided below, from the filtered warnings only.
   echo "WARN_AS_ERROR = NO"
} > "$TMPDIR_WORK/Doxyfile.check"

doxygen "$TMPDIR_WORK/Doxyfile.check" > "$TMPDIR_WORK/stdout.log" 2>&1
RC=$?

if [ $RC -ne 0 ]; then
   echo "dxm-check: doxygen failed (exit $RC)" >&2
   cat "$TMPDIR_WORK/stdout.log" >&2
   exit 2
fi

# Guard against the failure mode where doxygen runs happily but parsed nothing:
# no input means no warnings, which would look exactly like a clean module.
if [ ! -d "$TMPDIR_WORK/out/man" ] || [ -z "$(ls -A "$TMPDIR_WORK/out/man" 2>/dev/null)" ]; then
   echo "dxm-check: doxygen produced no output - it parsed no input files." >&2
   echo "dxm-check: refusing to report success. Check INPUT in Doxyfile." >&2
   exit 2
fi

# Keep only the warnings that belong to the directories under test. Paths in the
# log are absolute, so match on the resolved directory.
: > "$TMPDIR_WORK/filtered.log"
for abs in $ABS_DIRS; do
   grep -F "$abs/" "$TMPDIR_WORK/warnings.log" >> "$TMPDIR_WORK/filtered.log" 2>/dev/null
done
sort -u "$TMPDIR_WORK/filtered.log" -o "$TMPDIR_WORK/filtered.log"

if [ -s "$TMPDIR_WORK/filtered.log" ]; then
   cat "$TMPDIR_WORK/filtered.log"
   COUNT=$(grep -c . "$TMPDIR_WORK/filtered.log")
   echo "" >&2
   echo "dxm-check: $COUNT documentation defect(s) in: $*" >&2
   exit 1
fi

echo "dxm-check: clean - no documentation defects in: $*" >&2
exit 0
