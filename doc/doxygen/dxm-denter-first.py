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
CONTROL = re.compile(r'^(if|for|while|switch|do|else|case|default|return|goto|try|catch)\b')
CALL = re.compile(r'^[A-Za-z_]\w*(::\w*)*\s*\(')
IDENT = re.compile(r'[A-Za-z_]\w*')


def strip_noise(block):
    """Drop comments and string bodies from a block of lines.

    Classification has to run on code, not on prose: a declaration carrying a
    trailing comment ("size_t len;  /* length of a */") reads as unterminated,
    and a literal such as ",; " puts a semicolon where no statement ends. Both
    used to make the analysis give up on perfectly ordinary declarations.
    """
    out = []
    in_comment = False
    for line in block:
        buf = []
        i = 0
        while i < len(line):
            if in_comment:
                end = line.find('*/', i)
                if end < 0:
                    break
                in_comment = False
                i = end + 2
                continue
            ch = line[i]
            if ch in '"\'':
                j = i + 1
                while j < len(line) and line[j] != ch:
                    j += 2 if line[j] == '\\' else 1
                buf.append('@')                      # placeholder for a literal
                i = j + 1
                continue
            if line.startswith('//', i):
                break
            if line.startswith('/*', i):
                in_comment = True
                i += 2
                continue
            buf.append(ch)
            i += 1
        out.append(''.join(buf))
    return out


def statements(block):
    """Split a block into logical statements; the last one may be unterminated."""
    current = []
    for line in strip_noise(block):
        text = line.strip()
        if not text:
            continue
        current.append(text)
        joined = ' '.join(current)
        # a statement ends at a semicolon, but only once every bracket it opened
        # is closed again - otherwise a multi-line aggregate initialiser
        # ("static struct x tab[] = {\n {A, B},\n {C, D}\n};") would be torn into
        # fragments that look like anything but the declaration it is
        if (joined.endswith(';')
                and joined.count('(') == joined.count(')')
                and joined.count('{') == joined.count('}')
                and joined.count('[') == joined.count(']')):
            yield joined
            current = []
    if current:
        yield ' '.join(current)


def is_declaration(statement: str) -> bool:
    """True for a plain declaration - the only thing DENTER may be moved over."""
    s = statement.strip()
    if not s:
        return True
    if s.startswith('#'):
        return False                     # preprocessor: never reorder across it
    if CONTROL.match(s):
        return False
    if not s.endswith(';'):
        return False                     # unterminated: not a whole statement
    if s.startswith(('}', '{', '(')):
        return False                     # end of a type, a block, a cast expression
    if s.count('{') != s.count('}'):
        return False
    if CALL.match(s):
        return False                     # "foo(...);" is a call, not a declaration
    # a declaration names a type and then a variable, so there are at least two
    # identifiers left of any '='
    return len(IDENT.findall(s.split('=', 1)[0])) >= 2


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

        # Walk back to the opening brace of the body, counting brackets as we
        # go. Stopping at the first '{' seen is wrong: a local table
        # ("static struct f fields[] = {") opens one too, and taking that for
        # the function head made the initialiser itself look like a blocker.
        # The body brace is the first '{' that nothing below it closes again.
        j = i - 1
        depth = 0
        found = False
        while j >= 0 and i - j <= 40:
            code = strip_noise([out[j]])[0]
            depth += code.count('}') - code.count('{')
            if depth < 0:
                found = True
                break
            j -= 1

        if not found or j < 0:
            skipped.append((i + 1, 'no function body brace found within 40 lines'))
            i += 1
            continue
        if j == i - 1:
            i += 1                       # already first
            continue

        block = out[j + 1:i]

        # A DENTER inside a conditional branch must stay there: hoisting it
        # above the #if would turn a conditional trace into an unconditional
        # one. Detect it by the directives being unbalanced in the block.
        depth = 0
        for line in block:
            d = line.lstrip()
            if re.match(r'#\s*(if|ifdef|ifndef)\b', d):
                depth += 1
            elif re.match(r'#\s*endif\b', d):
                depth -= 1
        if depth != 0:
            skipped.append((i + 1, 'DENTER sits inside a preprocessor branch'))
            i += 1
            continue

        # Directives themselves are not statements - a #define or a balanced
        # #ifdef/#else/#endif around declarations does not execute anything, so
        # only what they enclose has to be judged.
        code = [l for l in block if not l.lstrip().startswith('#')]
        blockers = [s for s in statements(code) if not is_declaration(s)]
        if blockers:
            skipped.append((i + 1, 'not only declarations above: ' + blockers[0][:60]))
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
