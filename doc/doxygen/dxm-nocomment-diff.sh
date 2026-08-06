#!/bin/bash
#
# Prove that the working tree changed nothing but comments.
#
# For every modified C/C++ file under the given paths, this strips all comments
# from the committed revision and from the working copy, then diffs the two. A
# documentation-only change leaves no difference. Anything printed is a code
# change that slipped into a documentation commit.
#
# This is Gate A of the module conversion runbook. It is the check that makes a
# 3000 line comment diff reviewable: once this passes, the reviewer only has to
# judge whether the prose is right, not whether the code still works.
#
# Usage: dxm-nocomment-diff.sh [path ...]      (default: whole repository)
#
# Exit: 0  only comments changed
#       1  a code change was found
#       2  usage or environment problem
#
# See vault: 02 Work Projects/22 Doxygen Comment Migration/04 Module runbook.md

set -u

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
STRIPPER="$SCRIPT_DIR/dxm-strip-comments.py"

if [ ! -x "$STRIPPER" ] && [ ! -f "$STRIPPER" ]; then
   echo "dxm-nocomment-diff: cannot find $STRIPPER" >&2
   exit 2
fi

REPO_ROOT=$(git rev-parse --show-toplevel 2>/dev/null)
if [ -z "$REPO_ROOT" ]; then
   echo "dxm-nocomment-diff: not inside a git repository" >&2
   exit 2
fi
cd "$REPO_ROOT" || exit 2

if [ $# -eq 0 ]; then
   set -- .
fi

TMPDIR_WORK=$(mktemp -d) || exit 2
trap 'rm -rf "$TMPDIR_WORK"' EXIT

# Modified and staged files only. Added files have no committed revision to
# compare against and deleted files have no working copy; both are reported
# separately rather than silently ignored.
CHANGED=$(git diff --name-only --diff-filter=M HEAD -- "$@" |
          grep -E '\.(c|cc|cxx|cpp|h|hh|hpp|hxx)$')
ADDED=$(git diff --name-only --diff-filter=A HEAD -- "$@" |
        grep -E '\.(c|cc|cxx|cpp|h|hh|hpp|hxx)$')
DELETED=$(git diff --name-only --diff-filter=D HEAD -- "$@" |
          grep -E '\.(c|cc|cxx|cpp|h|hh|hpp|hxx)$')

FAILED=0
CHECKED=0

for f in $CHANGED; do
   CHECKED=$((CHECKED + 1))

   git show "HEAD:$f" > "$TMPDIR_WORK/head.src" 2>/dev/null || {
      echo "dxm-nocomment-diff: cannot read HEAD:$f" >&2
      FAILED=1
      continue
   }

   # Blank lines are dropped, not just trailing whitespace. The stripper leaves
   # a blank line behind for every comment line so that diffs stay aligned, but
   # this project's whole purpose is adding and removing large comment blocks -
   # keeping blank lines would report a moved #define every time a 240 line
   # banner above it is deleted. Comparing the sequence of non-blank code lines
   # still catches any added, removed or modified line of code.
   python3 "$STRIPPER" "$TMPDIR_WORK/head.src" |
      sed -e 's/[[:space:]]*$//' -e '/^$/d' > "$TMPDIR_WORK/head.stripped"
   python3 "$STRIPPER" "$f" |
      sed -e 's/[[:space:]]*$//' -e '/^$/d' > "$TMPDIR_WORK/work.stripped"

   if ! diff -u --label "a/$f (code only)" --label "b/$f (code only)" \
        "$TMPDIR_WORK/head.stripped" "$TMPDIR_WORK/work.stripped"; then
      FAILED=1
   fi
done

if [ -n "$ADDED" ]; then
   echo "# new files, no committed revision to compare - review by hand:" >&2
   for f in $ADDED; do echo "#   $f" >&2; done
fi
if [ -n "$DELETED" ]; then
   echo "# deleted files - review by hand:" >&2
   for f in $DELETED; do echo "#   $f" >&2; done
fi

if [ $FAILED -ne 0 ]; then
   echo "" >&2
   echo "dxm-nocomment-diff: CODE CHANGED - this is not a documentation-only diff" >&2
   exit 1
fi

echo "dxm-nocomment-diff: $CHECKED modified file(s), comments only" >&2
exit 0
