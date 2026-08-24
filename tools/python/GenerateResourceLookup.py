#!/usr/bin/env python3
#
# Read the (preprocessed) resources.txt and emit a
# name-to-ResourceId lookup table consumed by ResourceLookup.cpp.
#
# Supports comments:
#   #  ... whole-line comment (as before)
#   // ... rest of the line is ignored
#   /* ... */ block comments, also spanning multiple lines

import fileinput
import re
import sys

RE_BLANK = re.compile(r'^\s*(?:#.*)?$')
RE_PREPROC = re.compile(r'^\s*#\s*(?:if|ifdef|ifndef|elif|else|endif)\b')
RE_BITMAP = re.compile(r'^(?:bitmap_bitmap|bitmap_graphic|hatch_bitmap|app_icon)\s+([\w_]+)\s+"([^"]+)"\s*$')
RE_ICON_SCALED = re.compile(r'^bitmap_icon_scaled\s+([\w_]+)\s+"([^"]+)"\s*$')
RE_SOUND = re.compile(r'^sound\s+([\w_]+)\s+"([^"]+)"\s*$')


def strip_comments(line, in_block):
    """Remove //-line comments and /* ... */ block comments.

    Returns the stripped line and the (possibly updated) in_block state
    for block comments spanning multiple lines.
    """
    out = []
    i = 0
    while i < len(line):
        if in_block:
            end = line.find('*/', i)
            if end == -1:
                # block comment continues on the next line
                return ''.join(out), True
            i = end + 2
            in_block = False
        else:
            line_c = line.find('//', i)
            block_c = line.find('/*', i)
            if line_c != -1 and (block_c == -1 or line_c < block_c):
                # rest of the line is a comment
                out.append(line[i:line_c])
                return ''.join(out), False
            elif block_c != -1:
                out.append(line[i:block_c])
                i = block_c + 2
                in_block = True
            else:
                out.append(line[i:])
                break
    return ''.join(out), in_block


def main():
    in_block = False
    for line in fileinput.input():
        stripped, in_block = strip_comments(line, in_block)
        stripped = stripped.lstrip()

        # pass preprocessor conditionals (#if/#ifdef/#else/#endif ...) through
        if RE_PREPROC.match(stripped):
            print(stripped.rstrip())
            continue

        if RE_BLANK.match(stripped):
            continue

        m = RE_BITMAP.match(stripped)
        if m:
            name = m.group(1)
            print(f'  {{ "{name}", {name} }},')
            continue

        m = RE_ICON_SCALED.match(stripped)
        if m:
            name = m.group(1)
            print(f'  {{ "{name}", {name} }},')
            print(f'  {{ "{name}_MDPI", {name}_MDPI }},')
            print(f'  {{ "{name}_XHDPI", {name}_XHDPI }},')
            print(f'  {{ "{name}_XXHDPI", {name}_XXHDPI }},')
            continue

        if RE_SOUND.match(stripped):
            # sounds are not bitmap resources — skip
            continue

        sys.exit(f"Syntax error: {line}")


if __name__ == "__main__":
    main()