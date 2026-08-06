#!/usr/bin/env python3
"""Print a C/C++ source file with every comment removed.

Used by dxm-nocomment-diff.sh to prove that a documentation commit changed
nothing but comments: strip both revisions, diff the results, and any remaining
difference is a code change.

A regular expression cannot do this correctly. "/*" inside a string literal is
not a comment, '\\' is a backslash and not an escaped quote, a raw string can
contain anything at all, and a line comment ending in a backslash continues on
the next line. This is a character-level state machine instead.

Line structure is preserved: a line comment leaves its newline behind and a
block comment collapses to a single space, so the stripped output still lines up
with the original and a diff stays readable.

Usage: dxm-strip-comments.py FILE
       dxm-strip-comments.py -        (read stdin)
"""

import sys

CODE, LINE_COMMENT, BLOCK_COMMENT, STRING, CHAR, RAW_STRING = range(6)


def strip(src: str) -> str:
    out = []
    state = CODE
    i = 0
    n = len(src)
    raw_delim = ""

    while i < n:
        c = src[i]
        nxt = src[i + 1] if i + 1 < n else ""

        if state == CODE:
            # Raw string literal: R"delim( ... )delim" - no escapes inside.
            # Detect the R immediately before a quote, not preceded by an
            # identifier character (so `FOOR"x"` is not mistaken for a raw
            # string), which also covers the u8R / LR / uR / UR prefixes.
            if c == 'R' and nxt == '"' and (i == 0 or not (src[i - 1].isalnum() or src[i - 1] == '_')):
                close = src.find('(', i + 2)
                if close != -1:
                    raw_delim = ')' + src[i + 2:close] + '"'
                    out.append(src[i:close + 1])
                    i = close + 1
                    state = RAW_STRING
                    continue
            if c == '/' and nxt == '/':
                state = LINE_COMMENT
                i += 2
                continue
            if c == '/' and nxt == '*':
                state = BLOCK_COMMENT
                i += 2
                continue
            if c == '"':
                state = STRING
            elif c == "'":
                state = CHAR
            out.append(c)
            i += 1

        elif state == RAW_STRING:
            if src.startswith(raw_delim, i):
                out.append(raw_delim)
                i += len(raw_delim)
                state = CODE
                continue
            out.append(c)
            i += 1

        elif state in (STRING, CHAR):
            # A backslash escapes whatever follows it, including a quote or a
            # newline (line continuation inside a literal).
            if c == '\\' and i + 1 < n:
                out.append(src[i:i + 2])
                i += 2
                continue
            out.append(c)
            i += 1
            if (state == STRING and c == '"') or (state == CHAR and c == "'"):
                state = CODE

        elif state == LINE_COMMENT:
            # A line comment ending in a backslash continues onto the next line.
            if c == '\\' and nxt == '\n':
                out.append('\n')
                i += 2
                continue
            if c == '\\' and src.startswith('\r\n', i + 1):
                out.append('\n')
                i += 3
                continue
            if c == '\n':
                out.append('\n')
                i += 1
                state = CODE
                continue
            i += 1

        elif state == BLOCK_COMMENT:
            if c == '*' and nxt == '/':
                out.append(' ')
                i += 2
                state = CODE
                continue
            # Keep newlines so line numbers do not shift.
            if c == '\n':
                out.append('\n')
            i += 1

    return ''.join(out)


def main() -> int:
    if len(sys.argv) != 2:
        sys.stderr.write(__doc__)
        return 2
    path = sys.argv[1]
    if path == '-':
        src = sys.stdin.read()
    else:
        with open(path, encoding='utf-8', errors='surrogateescape') as fh:
            src = fh.read()
    sys.stdout.write(strip(src))
    return 0


if __name__ == '__main__':
    sys.exit(main())
