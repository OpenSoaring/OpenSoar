#!/usr/bin/env python3
"""Generate binary message catalog (.mo) from textual translation (.po).

Standalone replacement for GNU msgfmt / CPython's Tools/i18n/msgfmt.py,
kept in-tree because installed Pythons on Windows do not ship the Tools
directory.

Usage:
    msgfmt.py [--output-file=file.mo] file.po

Supports: msgctxt, msgid, msgid_plural, msgstr, msgstr[N], multiline
strings, C-style escapes, fuzzy-skipping.
"""

import os
import struct
import sys

__version__ = "1.0"


def unescape(s):
    """Resolve C-style escape sequences in a po string literal."""
    out = []
    i = 0
    n = len(s)
    escapes = {'n': '\n', 't': '\t', 'r': '\r', '"': '"', '\\': '\\',
               'a': '\a', 'b': '\b', 'f': '\f', 'v': '\v'}
    while i < n:
        c = s[i]
        if c == '\\' and i + 1 < n:
            nxt = s[i + 1]
            if nxt in escapes:
                out.append(escapes[nxt])
                i += 2
                continue
            if nxt == 'x' and i + 3 < n:
                out.append(chr(int(s[i + 2:i + 4], 16)))
                i += 4
                continue
            if nxt.isdigit():  # octal, up to 3 digits
                j = i + 1
                while j < n and j < i + 4 and s[j].isdigit():
                    j += 1
                out.append(chr(int(s[i + 1:j], 8)))
                i = j
                continue
        out.append(c)
        i += 1
    return ''.join(out)


def parse_po(lines, filename):
    """Yield (msgctxt, msgid, msgid_plural, msgstrs, fuzzy) tuples."""
    section = None  # None | 'ctxt' | 'id' | 'id_plural' | 'str'
    ctxt = msgid = plural = None
    msgstrs = {}
    fuzzy = False
    cur_index = 0

    def flush():
        nonlocal ctxt, msgid, plural, msgstrs, fuzzy, section
        if msgid is not None or msgstrs:
            yield_list.append((ctxt, msgid or '', plural,
                               [msgstrs[k] for k in sorted(msgstrs)], fuzzy))
        ctxt = msgid = plural = None
        msgstrs = {}
        fuzzy = False
        section = None

    yield_list = []
    for lno, line in enumerate(lines, 1):
        line = line.strip()
        if not line:
            flush()
            continue
        if line.startswith('#'):
            if line.startswith('#,') and 'fuzzy' in line:
                fuzzy = True
            continue
        if line.startswith('msgctxt'):
            flush()
            section = 'ctxt'
            ctxt = unescape(line[7:].strip().strip('"'))
            continue
        if line.startswith('msgid_plural'):
            section = 'id_plural'
            plural = unescape(line[12:].strip().strip('"'))
            continue
        if line.startswith('msgid'):
            if section in ('str',):
                flush()
            section = 'id'
            msgid = unescape(line[5:].strip().strip('"'))
            continue
        if line.startswith('msgstr['):
            section = 'str'
            close = line.index(']')
            cur_index = int(line[7:close])
            msgstrs[cur_index] = unescape(line[close + 1:].strip().strip('"'))
            continue
        if line.startswith('msgstr'):
            section = 'str'
            cur_index = 0
            msgstrs[0] = unescape(line[6:].strip().strip('"'))
            continue
        if line.startswith('"'):
            text = unescape(line.strip().strip('"'))
            if section == 'ctxt':
                ctxt += text
            elif section == 'id':
                msgid += text
            elif section == 'id_plural':
                plural += text
            elif section == 'str':
                msgstrs[cur_index] += text
            else:
                print('%s:%d: syntax error' % (filename, lno),
                      file=sys.stderr)
                sys.exit(1)
            continue
        print('%s:%d: unknown keyword: %r' % (filename, lno, line),
              file=sys.stderr)
        sys.exit(1)
    flush()
    return yield_list


def generate(entries):
    """Build the binary .mo content from parsed entries."""
    messages = {}
    for ctxt, msgid, plural, msgstrs, fuzzy in entries:
        if fuzzy and msgid:            # skip fuzzy entries (not the header)
            continue
        if not msgstrs or not any(msgstrs):
            continue                   # untranslated
        key = msgid
        if plural is not None:
            key = msgid + '\0' + plural
        if ctxt is not None:
            key = ctxt + '\x04' + key
        value = '\0'.join(msgstrs)
        messages[key.encode('utf-8')] = value.encode('utf-8')

    keys = sorted(messages)
    offsets = []
    ids = b''
    strs = b''
    for key in keys:
        val = messages[key]
        offsets.append((len(ids), len(key), len(strs), len(val)))
        ids += key + b'\0'
        strs += val + b'\0'

    n = len(keys)
    keystart = 7 * 4 + 16 * n
    valuestart = keystart + len(ids)
    koffsets = []
    voffsets = []
    for o1, l1, o2, l2 in offsets:
        koffsets += [l1, o1 + keystart]
        voffsets += [l2, o2 + valuestart]
    output = struct.pack('Iiiiiii',
                         0x950412de,        # magic
                         0,                 # version
                         n,                 # number of entries
                         7 * 4,             # start of key index
                         7 * 4 + n * 8,     # start of value index
                         0, 0)              # size/offset of hash table
    output += struct.pack('i' * n * 2, *koffsets)
    output += struct.pack('i' * n * 2, *voffsets)
    output += ids
    output += strs
    return output


def main():
    args = sys.argv[1:]
    outfile = None
    infile = None
    for arg in args:
        if arg.startswith('--output-file='):
            outfile = arg.split('=', 1)[1]
        elif arg in ('-h', '--help'):
            print(__doc__)
            return
        elif arg in ('-V', '--version'):
            print('msgfmt.py', __version__)
            return
        elif arg == '-o':
            outfile = '<next>'
        elif outfile == '<next>':
            outfile = arg
        else:
            infile = arg
    if not infile:
        print('No input file given', file=sys.stderr)
        sys.exit(1)
    if not outfile:
        outfile = os.path.splitext(infile)[0] + '.mo'

    with open(infile, encoding='utf-8') as f:
        lines = f.readlines()
    if lines and lines[0].startswith('﻿'):
        lines[0] = lines[0][1:]

    entries = parse_po(lines, infile)
    data = generate(entries)
    with open(outfile, 'wb') as f:
        f.write(data)


if __name__ == '__main__':
    main()
