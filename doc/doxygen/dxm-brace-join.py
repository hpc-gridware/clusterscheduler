#!/usr/bin/env python3
#
# Move a function body's opening brace onto the signature line: ") {".
#
# The checked-in .clang-format already asks for this (BraceWrapping /
# AfterFunction: false); ~2127 definitions predate it and still put the brace on
# a line of its own. Rather than reformat those files - which would rewrite
# indentation and wrapping everywhere and bury the change - this finds the
# offending brace lines and hands exactly those line ranges to clang-format.
# The tool then applies the project's own configuration to two lines and leaves
# the rest of the file untouched.
#
# Only braces that follow a closing parenthesis are considered, and only when
# the line does not begin with a control keyword, so if/for/while/switch and
# struct/class/enum bodies are never touched.
#
# Usage: dxm-brace-join.py [--apply] <file|dir> ...
#        without --apply it only reports (dry run)
#
# Environment: CLANG_FORMAT overrides the clang-format binary to use.
#
# Exit: 0  finished
#       2  usage or environment problem (no clang-format, no .clang-format)
#
# Run this before dxm-denter-first.py: that script anchors on the body brace
# and is simpler to reason about once the brace sits on the signature line.
#
# See vault: 02 Work Projects/22 Doxygen Comment Migration/

import os
import re
import shutil
import subprocess
import sys

CONTROL = re.compile(r'^\s*(if|for|while|switch|do|else|case|default|catch|return)\b')
COMMENT = re.compile(r'^\s*(//|/\*|\*)')
# a signature line ends in ')' possibly followed by qualifiers
SIG_END = re.compile(r'\)\s*(const|noexcept|override|final|\s)*$')

DEFAULT_CF = ('/home/ebablick/.local/share/JetBrains/Toolbox/apps/clion/'
              'plugins/clion-radler/DotFiles/linux-x64/clang-format')


def find_clang_format():
    cf = os.environ.get('CLANG_FORMAT')
    if cf and os.access(cf, os.X_OK):
        return cf
    cf = shutil.which('clang-format')
    if cf:
        return cf
    if os.access(DEFAULT_CF, os.X_OK):
        return DEFAULT_CF
    return None


def candidates(lines):
    """Line numbers (1-based) of lone '{' lines that close a function signature."""
    hits = []
    for i, line in enumerate(lines):
        if line.strip() != '{':
            continue
        j = i - 1
        while j >= 0 and (not lines[j].strip() or COMMENT.match(lines[j])):
            j -= 1
        if j < 0:
            continue
        prev = lines[j].rstrip()
        if not SIG_END.search(prev):
            continue
        if CONTROL.match(lines[j]) or lines[j].lstrip().startswith('#'):
            continue
        if j != i - 1:
            continue                     # comment or blank in between: leave it
        hits.append((j + 1, i + 1))
    return hits


# Vendored sources. clang-format would not merely move their brace: their
# indentation differs from ours, so touching one line re-indents whole function
# bodies and the diff stops being reviewable. cJSON cost 4387 lines before this
# list existed. The doxygen project excludes the same files, see note 06.
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
        print('usage: dxm-brace-join.py [--apply] <file|dir> ...', file=sys.stderr)
        return 2

    cf = find_clang_format()
    if not cf:
        print('dxm-brace-join: no clang-format found (set CLANG_FORMAT)', file=sys.stderr)
        return 2

    style = None
    for a in list(paths):
        if a.startswith('--style='):
            style = a.split('=', 1)[1]
            paths.remove(a)
    if style is None:
        # gcs-extensions carries no .clang-format of its own; it is built as one
        # product with clusterscheduler, so pass that one in with --style rather
        # than inventing a second house style.
        root = subprocess.run(['git', 'rev-parse', '--show-toplevel'],
                              capture_output=True, text=True).stdout.strip()
        style = os.path.join(root, '.clang-format')
    if not os.path.isfile(style):
        print(f'dxm-brace-join: {style} not found (use --style=<path>)', file=sys.stderr)
        return 2

    total = 0
    touched = 0
    for path in collect(paths):
        with open(path, encoding='utf-8', errors='surrogateescape') as fh:
            lines = fh.read().split('\n')
        hits = candidates(lines)
        if not hits:
            continue
        total += len(hits)
        touched += 1
        if not apply_changes:
            print(f'{path}: would join {len(hits)}')
            continue
        cmd = [cf, f'--style=file:{style}', '-i']
        for sig, brace in hits:
            cmd.append(f'--lines={sig}:{brace}')
        cmd.append(path)
        rc = subprocess.run(cmd, capture_output=True, text=True)
        if rc.returncode != 0:
            print(f'{path}: clang-format failed: {rc.stderr.strip()}', file=sys.stderr)
            return 2

    verb = 'joined' if apply_changes else 'would join'
    print(f'dxm-brace-join: {verb} {total} brace(s) in {touched} file(s)', file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
