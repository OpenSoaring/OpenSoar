#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright The XCSoar Project
#
# Reads NEWS.txt and writes a C++ header defining the Quick Guide "What's New"
# body as Markdown: first "# Version …" line is Heading 1, each "* section"
# line is Heading 2, with a blank line before and after each section title.
# Each NEWS bullet (``  - `` plus indented continuations) becomes one Markdown
# list item / paragraph so the in-app renderer keeps hanging indent.  ``#1234``
# issue references become GitHub links.

from __future__ import annotations

import argparse
import re
import sys

# GitHub issue links for NEWS “#1234” references (same tracker as the repo).
GITHUB_ISSUE_BASE = "https://github.com/XCSoar/XCSoar/issues"
# At least two digits, not ``#0`` / ``#1`` FLARM-style suffixes.  Skip
# ``#digits`` already used as link text in […](…), headings (``##…``),
# and bare ``#1234(`` fragments.
_ISSUE_REF = re.compile(r"(?<![#\[])#([1-9]\d+)\b(?!\()")


def extract_first_version_block(text: str) -> str:
    """Same scope as dlgQuickGuide TruncateToCurrentVersion: first Version line
    through the line before the next top-level ``Version ``."""
    lines = text.split("\n")
    if not lines:
        return ""
    out = [lines[0]]
    for i in range(1, len(lines)):
        if lines[i].startswith("Version "):
            break
        out.append(lines[i])
    return "\n".join(out)


def extract_first_opensoar_release(text: str) -> str:
    """The first release section of OpenSoar-News.md: from the first
    ``## OpenSoar `` heading up to the next ``## `` heading."""
    out: list[str] = []
    for line in text.split("\n"):
        if line.startswith("## "):
            if out:
                break
            if line.startswith("## OpenSoar "):
                out.append(line)
            continue
        if out:
            out.append(line)
    return "\n".join(out)


def convert_opensoar_section_to_markdown(block: str) -> str:
    """OpenSoar-News.md is Markdown already, but written for a text
    viewer: list items wrap over indented lines and code spans use
    backticks.  The in-app renderer wants one line per list item or
    paragraph, so the same shape as convert_news_block_to_markdown()
    is produced: the release heading as Heading 1, ``###`` headings as
    Heading 2, every ``* `` item and every nested ``  - `` item as one
    list item, and every other paragraph as one line."""
    lines = block.replace("`", "").split("\n")
    md: list[str] = []
    if lines:
        md.append("# " + lines[0][3:].strip())
        md.append("")

    i = 1
    while i < len(lines):
        line = lines[i].rstrip("\r")
        if line.strip() == "" or line.strip() == "---":
            if md and md[-1] != "":
                md.append("")
            i += 1
            continue

        if line.startswith("### "):
            if md and md[-1] != "":
                md.append("")
            md.append("## " + line[4:].strip())
            md.append("")
            i += 1
            continue

        if line.startswith("* ") or line.startswith("  - "):
            nested = line.startswith("  - ")
            cont = "    " if nested else "  "
            parts = [line[4:].strip() if nested else line[2:].strip()]
            i += 1
            while i < len(lines):
                nl = lines[i]
                if (nl.strip() == "" or nl.startswith("* ") or
                        nl.startswith("  - ") or not nl.startswith(cont)):
                    break
                parts.append(nl.strip())
                i += 1
            md.append("- " + " ".join(parts))
            continue

        # a plain paragraph: everything up to the next blank line or item
        parts = [line.strip()]
        i += 1
        while i < len(lines):
            nl = lines[i]
            if (nl.strip() == "" or nl.startswith("* ") or
                    nl.startswith("  - ") or nl.startswith("#")):
                break
            parts.append(nl.strip())
            i += 1
        md.append(" ".join(parts))

    while md and md[-1] == "":
        md.pop()
    return linkify_github_issue_refs("\n".join(md) + "\n")


def linkify_github_issue_refs(text: str) -> str:
    """Turn ``#1234`` into ``[#1234](https://github.com/.../issues/1234)``."""

    def repl(m):
        n = m.group(1)
        return f"[#{n}]({GITHUB_ISSUE_BASE}/{n})"

    return _ISSUE_REF.sub(repl, text)


def convert_news_block_to_markdown(block: str) -> str:
    lines = block.split("\n")
    md: list[str] = []
    i = 0
    if i < len(lines):
        first = lines[i].strip()
        if first:
            md.append("# " + first if not first.startswith("# ") else first)
            md.append("")  # blank line after version heading
        i += 1

    while i < len(lines):
        raw = lines[i]
        line = raw.rstrip("\r")

        if line.strip() == "":
            md.append("")
            i += 1
            continue

        stripped = line.lstrip()

        # Section title: single * at line start (NEWS convention), not a bullet.
        if (
            stripped.startswith("* ")
            and not line.startswith("  ")
            and not stripped.startswith("* *")
        ):
            if md and md[-1] != "":
                md.append("")  # blank line before section heading
            md.append("## " + stripped[2:].strip())
            md.append("")  # blank line after section heading
            i += 1
            continue

        # Bullet with optional indented continuations (4 spaces) → one
        # Markdown list item (one paragraph in the rich-text renderer).
        if line.startswith("  - "):
            parts: list[str] = [line[4:].strip()]
            i += 1
            while i < len(lines):
                nl = lines[i]
                if nl.strip() == "":
                    break
                if nl.startswith("    ") and nl[4:].strip():
                    parts.append(nl[4:].strip())
                    i += 1
                    continue
                break
            md.append("- " + " ".join(parts))
            continue

        md.append(line)
        i += 1

    # Trim trailing blank lines; ensure trailing newline
    while md and md[-1] == "":
        md.pop()
    return linkify_github_issue_refs("\n".join(md) + "\n")


def cxx_escape(s: str) -> str:
    return (
        s.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\r", "\\r")
    )


def cxx_string_chunks(s: str, chunk: int = 2048) -> str:
    """Adjacent C++ string literals; file is UTF-8.

    Slice the raw input first and escape each chunk independently so that
    escape sequences (\\n, \\", \\\\) are never split across chunk
    boundaries (which would produce invalid C++ literals).
    """
    out: list[str] = []
    for j in range(0, len(s), chunk):
        out.append('"' + cxx_escape(s[j : j + chunk]) + '"')
    return "\n  ".join(out) if out else '""'


def emit_header(markdown: str) -> None:
    chunks = cxx_string_chunks(markdown)
    print("// SPDX-License-Identifier: GPL-2.0-or-later")
    print("// Copyright The XCSoar Project")
    print("")
    print("// Generated by tools/news_to_quickguide_md.py - do not edit.")
    print("")
    print("#pragma once")
    print("")
    print("/** Markdown body for Quick Guide What's New (current version). */")
    print("inline constexpr char quick_guide_news_markdown[] =")
    print(f"  {chunks};")


def main() -> int:
    # the header is written to stdout: force UTF-8 there - on Windows
    # the default console encoding is cp1252, which cannot represent
    # characters like the arrows used in NEWS.txt
    if sys.stdout.encoding and sys.stdout.encoding.lower() not in ("utf-8", "utf8"):
        sys.stdout.reconfigure(encoding="utf-8")
    p = argparse.ArgumentParser()
    p.add_argument(
        "news_file",
        nargs="?",
        default="NEWS.txt",
        help="Path to NEWS.txt (default: NEWS.txt)",
    )
    p.add_argument(
        "--opensoar-news",
        metavar="FILE",
        help="OpenSoar-News.md; when given, the page shows its first "
             "release section instead of the first block of NEWS.txt",
    )
    args = p.parse_args()

    # An OpenSoar build shows the OpenSoar news of the release: the
    # first block of NEWS.txt belongs to the next XCSoar version, which
    # is often still "not yet released" and names none of the OpenSoar
    # changes.  NEWS.txt stays the fallback, e.g. for an unbranded tree.
    if args.opensoar_news:
        try:
            with open(args.opensoar_news, encoding="utf-8") as f:
                block = extract_first_opensoar_release(f.read())
        except OSError as e:
            print(f"news_to_quickguide_md: {e}", file=sys.stderr)
            return 1
        if block.strip():
            md = convert_opensoar_section_to_markdown(block)
            if md.strip():
                emit_header(md)
                return 0

    try:
        with open(args.news_file, encoding="utf-8") as f:
            text = f.read()
    except OSError as e:
        print(f"news_to_quickguide_md: {e}", file=sys.stderr)
        return 1

    block = extract_first_version_block(text)
    if not block.strip():
        print("news_to_quickguide_md: empty first version block", file=sys.stderr)
        return 1

    md = convert_news_block_to_markdown(block)
    if not md.strip():
        print("news_to_quickguide_md: conversion produced empty markdown", file=sys.stderr)
        return 1

    emit_header(md)
    return 0


if __name__ == "__main__":
    sys.exit(main())
