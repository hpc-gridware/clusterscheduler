#!/usr/bin/env python3
"""Convert legacy GE banner headers into doxygen blocks.

    dxm-convert-banners.py FILE [FILE ...]

This is a migration aid, not a formatter. It does the mechanical part of the
conversion - splitting a banner into its NAME/FUNCTION/INPUTS/RESULT/NOTES/
SEE ALSO sections and re-emitting them as @brief/@param/@return/@note/@see - so
that a human or an agent can spend its attention on the part that actually needs
judgement: checking the prose against the code.

What it deliberately does NOT do, because each needs a decision:

  * Module banners (the ones whose name starts with '-', e.g. --Bitfield or
    -Bitfield_Typedefs) are left untouched and reported. They become @defgroup
    or @page and the choice of which, and where, is not mechanical.
  * @param thiz and friends are not invented. A banner that predates a refactor
    lists the wrong parameters; the gate reports that afterwards.
  * SYNOPSIS is dropped - doxygen renders the real signature - but it is not
    checked against the code first.

After running this, ALWAYS:

  1. re-read the prose against the function body, and
  2. run the gate with EXTRACT_STATIC = YES to get the list of @param
     mismatches, missing @return and undocumented functions.

Exit status is 0 even when nothing was converted; the summary is on stderr.
"""

import re
import sys

SECTION_RE = re.compile(r'^  ([A-Z][A-Z /]*[A-Z])\s*$')
BANNER_RE = re.compile(r'^/\*{5,}\s+(\S+)')

# Section name variants seen in the tree, mapped to the canonical name.
CANON = {
    'INPUT': 'INPUTS', 'INPUTS': 'INPUTS',
    'RESULT': 'RESULT', 'RESULTS': 'RESULT', 'RETURN': 'RESULT',
    'RETURNS': 'RESULT', 'RETURN VALUES': 'RESULT',
    'NOTE': 'NOTES', 'NOTES': 'NOTES', 'MUTEXES': 'NOTES',
    'OUTPUT': 'OUTPUTS', 'OUTPUTS': 'OUTPUTS',
    'EXAMPLE': 'EXAMPLE', 'EXAMPLES': 'EXAMPLE',
    'NAME': 'NAME', 'FUNCTION': 'FUNCTION', 'SYNOPSIS': 'SYNOPSIS',
    'SEE ALSO': 'SEE ALSO', 'BUGS': 'BUGS', 'TODO': 'TODO',
}


def parse_sections(block):
    secs, cur = {}, None
    for line in block:
        body = line[1:] if line.startswith('*') else line
        m = SECTION_RE.match(body.rstrip())
        if m:
            cur = CANON.get(m.group(1).strip())
            if cur:
                secs.setdefault(cur, [])
        elif cur:
            secs[cur].append(body[5:] if body.startswith('     ') else body.strip())
    return secs


def is_placeholder(text):
    """Banners in this tree use '???' where the author never wrote anything."""
    return not text or not text.strip().strip('?').strip()


def trim(lines):
    lines = list(lines)
    while lines and not lines[0].strip():
        lines.pop(0)
    while lines and not lines[-1].strip():
        lines.pop()
    return lines


def see_also(entry, local_symbols):
    """module/path/name() -> #name when the symbol is local, else `name()`."""
    e = entry.strip()
    if not e:
        return None
    m = re.match(r'^[\w\- /]*?([A-Za-z_]\w*)\(\)$', e)
    if m:
        name = m.group(1)
        return ('#' + name) if name in local_symbols else '`%s()`' % name
    return None


def convert_block(block, local_symbols, returns_void=False):
    s = parse_sections(block)
    out = []

    brief = ''
    for line in s.get('NAME', []):
        if '--' in line:
            brief = line.split('--', 1)[1].strip()
            break
    if is_placeholder(brief):
        fn = [x for x in trim(s.get('FUNCTION', [])) if not is_placeholder(x)]
        brief = fn[0] if fn else 'TODO document this'
    brief = brief.rstrip('. ')
    if brief:
        brief = brief[0].upper() + brief[1:]
    out.append(' * @brief ' + brief)

    fn = [x for x in trim(s.get('FUNCTION', [])) if not is_placeholder(x)]
    if fn and fn[0].strip() != brief:
        out.append(' *')
        out.extend((' * ' + line).rstrip() for line in fn)

    ex = [x for x in trim(s.get('EXAMPLE', [])) if not is_placeholder(x)]
    if ex:
        out.append(' *')
        out.append(' * @code')
        out.extend((' * ' + line).rstrip() for line in ex)
        out.append(' * @endcode')

    params = []
    for line in trim(s.get('INPUTS', [])) + trim(s.get('OUTPUTS', [])):
        m = re.match(r'^\s*(?:[\w \*\[\]]+?[ \*])?(\w+)\s+-\s+(.*)$', line)
        if m:
            # "void - ???" documents the absence of parameters, not a parameter
            # called void; @param void is a doxygen warning.
            if m.group(1) == 'void':
                continue
            params.append([m.group(1), m.group(2).strip()])
        elif params and line.strip():
            params[-1][1] += ' ' + line.strip()
    if params:
        out.append(' *')
        for name, desc in params:
            desc = '' if is_placeholder(desc) else desc
            out.append((' * @param %s %s' % (name, desc)).rstrip())

    ret = trim(s.get('RESULT', []))
    mt_note = None
    if ret:
        # Some banners put the MT-NOTE inside RESULT instead of NOTES, where it
        # would otherwise be appended to the @return text.
        keep = []
        for line in ret:
            if re.match(r'^\s*MT-NOTE\b', line):
                mt_note = line.strip()
            else:
                keep.append(line)
        ret = trim(keep)
    if ret:
        txt = ' '.join(x.strip() for x in ret if x.strip())
        # A RESULT section on a void function still describes something, but
        # @return on a function that returns nothing is a doxygen warning.
        # Drop the boilerplate ones and demote the rest to a @note.
        m = re.match(r'^void\s*(?:-\s*(.*))?$', txt)
        if m or returns_void:
            rest = (m.group(1) if m else txt) or ''
            rest = rest.strip().rstrip('.')
            if rest and rest.lower() not in ('none', 'nothing', 'no result'):
                out.append(' *')
                out.append(' * @note ' + rest)
        else:
            txt = re.sub(r'^[\w \*]+?\s*-\s*', '', txt)
            if is_placeholder(txt):
                txt = 'TODO document the return value'
            out.append(' *')
            out.append(' * @return ' + txt)
    if mt_note:
        out.append(' *')
        out.append(' * @note ' + mt_note)

    notes = trim(s.get('NOTES', []))
    if notes:
        out.append(' *')
        out.append(' * @note ' + notes[0].strip())
        out.extend((' *       ' + x.strip()).rstrip() for x in notes[1:])

    for key, tag in (('BUGS', '@bug'), ('TODO', '@todo')):
        body = trim(s.get(key, []))
        if body:
            out.append(' *')
            out.append(' * %s %s' % (tag, body[0].strip()))
            out.extend((' *      ' + x.strip()).rstrip() for x in body[1:])

    refs = [see_also(x, local_symbols) for x in trim(s.get('SEE ALSO', []))]
    refs = [r for r in dict.fromkeys(r for r in refs if r)]
    if refs:
        out.append(' *')
        out.append(' * @see ' + ', '.join(refs))

    return ['/**'] + [placeholders(x) for x in out] + [' */']


def placeholders(line):
    """`<name>` is banner-speak for "the parameter called name".

    Doxygen reads it as an HTML tag instead and warns about every one of them,
    so turn it into inline code. A '<' that follows an identifier character is
    left alone - that is a template argument such as vector<int>, not a
    placeholder.
    """
    return re.sub(r'(?<![\w>])<([A-Za-z_]\w*)>', r'`\1`', line)


def local_symbol_names(text):
    """Function-ish names defined or declared in this file."""
    return set(re.findall(r'^[\w:<>,\* &\[\]]*?\b(\w+)\s*\(', text, re.M))


def returns_void(lines, start):
    """Does the declaration following a banner return void?

    A RESULT section is not reliable here: plenty of banners describe what a
    void function changed ("'this_range' will be modified") without naming the
    type, and @return on a void function is a doxygen warning. The signature is
    the only thing that actually knows.
    """
    for line in lines[start:start + 4]:
        if not line.strip():
            continue
        sig = line.strip()
        # The return type is sometimes on a line of its own, above the name.
        return re.match(r'^(static\s+)?void(\s+[\w:]+\s*\(|\s*$)', sig) is not None
    return False


def process(path):
    text = open(path, encoding='utf-8', errors='surrogateescape').read()
    lines = text.split('\n')
    symbols = local_symbol_names(text)

    out, i, converted, skipped = [], 0, 0, []
    while i < len(lines):
        m = BANNER_RE.match(lines[i])
        if m and lines[i].rstrip().endswith('*'):
            name = m.group(1).rstrip('()').split('/')[-1]
            j = i + 1
            while j < len(lines) and not lines[j].rstrip().endswith('*/'):
                j += 1
            if j >= len(lines):
                out.append(lines[i]); i += 1; continue
            if name.startswith('-'):
                skipped.append(name)
                out.extend(lines[i:j + 1])
            else:
                out.extend(convert_block(lines[i + 1:j], symbols,
                                         returns_void(lines, j + 1)))
                converted += 1
            i = j + 1
            continue
        out.append(lines[i])
        i += 1

    if converted:
        open(path, 'w', encoding='utf-8', errors='surrogateescape').write('\n'.join(out))
    return converted, skipped


def main():
    if len(sys.argv) < 2:
        sys.stderr.write(__doc__)
        return 2
    total, all_skipped = 0, []
    for path in sys.argv[1:]:
        n, skipped = process(path)
        total += n
        for s in skipped:
            all_skipped.append('%s: %s' % (path, s))
        sys.stderr.write('%-50s %3d converted, %d module banners left\n'
                         % (path, n, len(skipped)))
    if all_skipped:
        sys.stderr.write('\nModule banners needing a manual @defgroup / @page decision:\n')
        for s in all_skipped:
            sys.stderr.write('  %s\n' % s)
    sys.stderr.write('\ntotal converted: %d\n' % total)
    return 0


if __name__ == '__main__':
    sys.exit(main())
