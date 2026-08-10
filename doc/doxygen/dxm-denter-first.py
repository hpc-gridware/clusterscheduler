#!/usr/bin/env python3
#
# Make DENTER() the first statement of its function body.
#
# DENTER's own documentation (sge_rmon_macros.h) says it "must be the first
# statement of the function", but in ~1660 places it sits after the first
# variable declarations. That falsifies the trace: a declaration initialised
# from another function's return value runs - and traces its own entry and exit
# - before this function has announced that it was entered at all, so the
# nesting in the trace shows the callee outside its caller.
#
# This moves the DENTER line up to the top of the body. It only does so when
# everything it would jump over is a plain declaration, which is the case that
# is safe by construction: declarations have no control flow, so the only thing
# that changes is when their initialisers run relative to the trace entry - and
# running them after the trace entry is the point.
#
# Anything else - a lock, an if, a preprocessor branch, a return - is left
# alone and reported. Those are decisions, not mechanics.
#
# Usage: dxm-denter-first.py [--apply] <file|dir> ...
#        without --apply it only reports (dry run)
#
# Exit: 0  finished (see the summary for skipped sites)
#       2  usage or environment problem
#
# Run dxm-brace-join.py first: this anchors on the body's opening brace, which
# that script has by then moved onto the signature line.
#
# See vault: 02 Work Projects/22 Doxygen Comment Migration/

import os
import re
import sys

DENTER = re.compile(r'^(\s*)DENTER(_MAIN|_)?\s*\(.*\)\s*;\s*$')
# a line that only declares/initialises - no control flow, no nesting
DECL = re.compile(r'^\s*[A-Za-z_][^;{}]*;\s*$')
CONTROL = re.compile(r'^\s*(if|for|while|switch|do|else|case|default|return|goto|try|catch)\b')
COMMENT = re.compile(r'^\s*(//|/\*|\*)')


def is_declaration(line: str) -> bool:
    s = line.strip()
    if not s or COMMENT.match(line):
        return True                      # blanks and comments may be jumped over
    if s.startswith('#'):
        return False                     # preprocessor: never reorder across it
    if CONTROL.match(line):
        return False
    # A brace initialiser is still a declaration: "SGE_STRUCT_STAT buf{};" or
    # "std::vector<int> v{1, 2};". Only balanced braces on the line qualify -
    # an unbalanced one opens a block and must stop us.
    if '{' in s or '}' in s:
        if s.count('{') != s.count('}') or not s.endswith(';'):
            return False
        s = re.sub(r'\{[^{}]*\}', '', s)
        if '{' in s or '}' in s:
            return False
        return True
    return bool(DECL.match(line))


def process(lines):
    """Return (new_lines, moved, skipped) - skipped is a list of (lineno, why)."""
    out = list(lines)
    moved = 0
    skipped = []
    i = 0
    while i < len(out):
        m = DENTER.match(out[i])
        if not m:
            i += 1
            continue

        # walk back to the opening brace of the body
        j = i - 1
        blockers = []
        while j >= 0 and not out[j].rstrip().endswith('{'):
            if out[j].strip() and not COMMENT.match(out[j]):
                if not is_declaration(out[j]):
                    blockers.append(out[j].strip()[:60])
            j -= 1
            if i - j > 40:               # not a function head within reach
                break

        if j < 0 or not out[j].rstrip().endswith('{'):
            skipped.append((i + 1, 'no function body brace found within 40 lines'))
            i += 1
            continue
        if j == i - 1:
            i += 1                       # already first
            continue
        if blockers:
            skipped.append((i + 1, 'not only declarations above: ' + blockers[-1]))
            i += 1
            continue

        denter_line = out[i]
        # take a single trailing blank with it, so no gap is left behind
        end = i + 1
        if end < len(out) and not out[end].strip():
            end += 1
        del out[i:end]
        out.insert(j + 1, denter_line)
        if j + 2 < len(out) and out[j + 2].strip():
            out.insert(j + 2, '')
        moved += 1
        i = j + 2
    return out, moved, skipped


# Vendored sources, kept in step with dxm-brace-join.py. They carry no DENTER
# today, but nothing should start rewriting them by accident either.
VENDORED = ('cJSON.c', 'cJSON.h')


def collect(paths):
    files = []
    for p in paths:
        if os.path.isfile(p):
            files.append(p)
            continue
        for d, dirs, fs in os.walk(p):
            dirs[:] = [x for x in dirs if x not in
                       ('cmake-build-debug', 'cmake-build-release', '3rdparty', '.git')]
            files += [os.path.join(d, f) for f in fs
                      if f.endswith(('.c', '.cc', '.h', '.hpp')) and f not in VENDORED]
    return files


def main() -> int:
    args = sys.argv[1:]
    apply_changes = '--apply' in args
    paths = [a for a in args if a != '--apply']
    if not paths:
        print(__doc__ or 'usage: dxm-denter-first.py [--apply] <file|dir> ...', file=sys.stderr)
        return 2

    total_moved = 0
    total_skipped = 0
    touched = 0
    for path in collect(paths):
        with open(path, encoding='utf-8', errors='surrogateescape') as fh:
            original = fh.read()
        lines = original.split('\n')
        new, moved, skipped = process(lines)
        for lineno, why in skipped:
            print(f'{path}:{lineno}: SKIPPED - {why}')
        total_skipped += len(skipped)
        if moved:
            total_moved += moved
            touched += 1
            if apply_changes:
                with open(path, 'w', encoding='utf-8', errors='surrogateescape') as fh:
                    fh.write('\n'.join(new))
            else:
                print(f'{path}: would move {moved}')

    verb = 'moved' if apply_changes else 'would move'
    print(f'dxm-denter-first: {verb} {total_moved} DENTER call(s) in {touched} file(s), '
          f'{total_skipped} skipped for review', file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
