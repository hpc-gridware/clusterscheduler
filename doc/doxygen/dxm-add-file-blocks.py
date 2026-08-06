#!/usr/bin/env python3
"""Insert a /** @file */ block into C/C++ files that lack one.

    dxm-add-file-blocks.py [-b BRIEF] FILE [FILE ...]

Why this exists: without an @file block doxygen reports nothing at all for a
file - not undocumented functions, not missing @param tags - so the gate cannot
see it. Adding the block is therefore the first step when converting a file, and
it has to happen for every file in the module.

The block is placed immediately after the license header
(/*___INFO__MARK_END__*/ or the _NEW__ variant) and before the first #include,
which is where the house style wants it. The license header itself is never
touched.

Without -b, a placeholder brief is written that names the file. That is
deliberate: the placeholder is easy to grep for and the gate does not accept a
file whose brief was never written properly, so it cannot be forgotten silently.

Files that already contain an @file block are left alone.
"""

import os
import re
import sys

PLACEHOLDER = 'TODO describe this file'


def insert(path, brief):
    text = open(path, encoding='utf-8', errors='surrogateescape').read()
    if re.search(r'@file\b|\\file\b', text):
        return False

    block = '\n/** @file\n * @brief %s\n */\n' % (brief or PLACEHOLDER)

    m = re.search(r'/\*___INFO__MARK_END(?:_NEW)?__\*/\n', text)
    if m:
        pos = m.end()
    else:
        # No license header: put it above the first preprocessor line instead.
        m = re.search(r'^\s*#\s*(include|pragma|ifndef|define)', text, re.M)
        pos = m.start() if m else 0

    open(path, 'w', encoding='utf-8', errors='surrogateescape').write(
        text[:pos] + block + text[pos:])
    return True


def main():
    args = sys.argv[1:]
    brief = None
    if len(args) >= 2 and args[0] == '-b':
        brief, args = args[1], args[2:]
    if not args:
        sys.stderr.write(__doc__)
        return 2

    added = skipped = 0
    for path in args:
        if insert(path, brief):
            added += 1
        else:
            skipped += 1
    sys.stderr.write('@file blocks added: %d, already present: %d\n' % (added, skipped))
    if brief is None and added:
        sys.stderr.write('placeholder used - grep for "%s" and write real briefs\n'
                         % PLACEHOLDER)
    return 0


if __name__ == '__main__':
    sys.exit(main())
