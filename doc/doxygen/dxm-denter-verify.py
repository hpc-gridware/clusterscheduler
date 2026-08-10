#!/usr/bin/env python3
#
# Prove that a DENTER/brace pass changed nothing but whitespace and the
# position of the DENTER lines.
#
# This is the counterpart to dxm-nocomment-diff.sh, which proves a comment-only
# change. That check cannot be used here: this pass moves a statement, so the
# code genuinely differs. What must hold instead is narrower and still strong -
# with the DENTER lines removed and all whitespace collapsed, the two revisions
# must be character for character identical. That leaves exactly two degrees of
# freedom, which are the two things the pass is allowed to do: join the body
# brace onto the signature line, and move DENTER to the top of the body.
#
# Anything else - a dropped line, a mangled declaration, a brace joined onto the
# wrong statement - changes the token stream and is reported.
#
# It compares the working tree against a git revision, so run it before
# committing, or pass the revision the pass started from.
#
# Usage: dxm-denter-verify.py [revision] [path ...]      (default: HEAD, all changed files)
#
# Exit: 0  only whitespace and DENTER position changed
#       1  a real code change was found
#       2  usage or environment problem
#
# See vault: 02 Work Projects/22 Doxygen Comment Migration/

import re
import subprocess
import sys

DENTER = re.compile(r'^\s*DENTER(_MAIN|_)?\s*\(.*\)\s*;\s*$')
WS = re.compile(r'\s+')


def canonical(text: str) -> str:
    """Whole-file token stream, DENTER lines removed, all whitespace gone."""
    kept = [l for l in text.split('\n') if not DENTER.match(l)]
    return WS.sub('', '\n'.join(kept))


def main() -> int:
    args = sys.argv[1:]
    rev = 'HEAD'
    if args and not args[0].startswith('-') and '/' not in args[0]:
        rev = args.pop(0)

    cmd = ['git', 'diff', '--name-only', rev, '--'] + args
    changed = subprocess.run(cmd, capture_output=True, text=True)
    if changed.returncode != 0:
        print(f'dxm-denter-verify: {changed.stderr.strip()}', file=sys.stderr)
        return 2

    files = [f for f in changed.stdout.split()
             if f.endswith(('.c', '.cc', '.h', '.hpp'))]
    if not files:
        print('dxm-denter-verify: no changed C/C++ files', file=sys.stderr)
        return 0

    bad = []
    for f in files:
        old = subprocess.run(['git', 'show', f'{rev}:{f}'],
                             capture_output=True, text=True)
        if old.returncode != 0:
            continue                              # newly added file
        try:
            with open(f, encoding='utf-8', errors='surrogateescape') as fh:
                new = fh.read()
        except FileNotFoundError:
            continue                              # deleted
        if canonical(old.stdout) != canonical(new):
            bad.append(f)

    for f in bad:
        print(f'{f}: CODE CHANGED - not just whitespace and DENTER position')
    if bad:
        print(f'dxm-denter-verify: {len(bad)} of {len(files)} file(s) changed beyond '
              f'whitespace and DENTER position', file=sys.stderr)
        return 1
    print(f'dxm-denter-verify: {len(files)} file(s), whitespace and DENTER position only',
          file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
