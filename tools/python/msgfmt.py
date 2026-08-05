#!/usr/bin/env python3
import sys
import struct

def parse_po(filename):
    messages = {}
    msgid = msgstr = None
    state = None

    with open(filename, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line.startswith("msgid "):
                msgid = line[6:].strip().strip('"')
                state = "msgid"
            elif line.startswith("msgstr "):
                msgstr = line[7:].strip().strip('"')
                state = "msgstr"
                messages[msgid] = msgstr
            elif line.startswith('"'):
                if state == "msgid":
                    msgid += line.strip('"')
                elif state == "msgstr":
                    msgstr += line.strip('"')
                    messages[msgid] = msgstr
    return messages

def write_mo(messages, filename):
    keys = sorted(messages.keys())
    offsets = []
    ids = strs = b""

    for k in keys:
        id_bytes = k.encode("utf-8")
        str_bytes = messages[k].encode("utf-8")
        offsets.append((len(id_bytes), len(ids), len(str_bytes), len(strs)))
        ids += id_bytes + b"\0"
        strs += str_bytes + b"\0"

    # Header
    header = struct.pack("Iiiiiii",
        0x950412de,  # magic
        0,           # version
        len(keys),   # number of strings
        28,          # offset of table with original strings
        28 + len(keys) * 8,  # offset of table with translated strings
        0, 0         # hash table offset + size
    )

    # Build tables
    orig_table = b""
    trans_table = b""
    for length, offset, tlength, toffset in offsets:
        orig_table += struct.pack("II", length, offset + 28 + len(keys) * 16)
        trans_table += struct.pack("II", tlength, toffset + 28 + len(keys) * 16 + len(ids))

    with open(filename, "wb") as f:
        f.write(header)
        f.write(orig_table)
        f.write(trans_table)
        f.write(ids)
        f.write(strs)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python po2mo.py input.po output.mo")
        sys.exit(1)

    messages = parse_po(sys.argv[1])
    write_mo(messages, sys.argv[2])
