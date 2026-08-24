#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright The XCSoar Project
"""
Non-Win32 resource pipeline (SDL/OpenGL flavor) for the CMake build.

Upstream (build/resource.mk + tools/LinkResources.pl + GenerateResources.pl)
embeds all bitmaps as PNG and all sounds as raw PCM into 'resources.c' and
generates 'resource_data.h' with the named (sound) resource table.  This
script does the same in one step, incrementally:

  1. preprocess Data/resources.txt (#ifdef/#ifndef/#else/#endif + defines)
  2. render every referenced PNG / raw file that is missing or outdated:
       bitmap_icon_scaled  X "f"  -> icons/f_{96,160,300}.png   (svg, zoom
                                     1.0 / 1.6316 / 3.0 = 96/156.6/288 dpi,
                                     via build/svg_preprocess.xsl like upstream)
       bitmap_graphic      X "f"  -> graphics2/f.png  (recipe by name, see
                                     GRAPHIC_RECIPES: logos, titles, launcher
                                     halves, gestures, disclosure icons, ...)
       bitmap_bitmap       X "f"  -> bitmaps/f.png    (grayscale from .bmp)
       sound               X "f"  -> sound/f.raw      (16 bit / 44100 Hz mono)
       app_icon / hatch_bitmap    -> Win32 only, skipped
  3. write resources.c   (C byte arrays: resource_X[], resource_X_size)
     write resource_data.h (named_resources[] table for the sounds)

Tools: inkscape (svg -> png), ImageMagick convert (bmp -> png, crops),
       xsltproc (svg preprocessing; optional - falls back to the raw svg)

Usage:
  gen_resources_data.py --src <repo> --data <data-out-dir>
        --out-c <resources.c> --out-h <resource_data.h>
        [--define XCSOAR_TESTING --define ENABLE_OPENGL ...]
        [--inkscape PATH] [--convert PATH] [--xsltproc PATH]
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import struct
import subprocess
import sys
import wave

ICON_ZOOM = {"96": 1.0, "160": 1.6316, "300": 3.0}


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

def preprocess(path: str, defines: set[str]) -> list[str]:
    """tiny #ifdef/#ifndef/#else/#endif preprocessor (nested)"""
    out: list[str] = []
    stack: list[bool] = []          # 'currently emitting' per nesting level
    with open(path, encoding="utf-8") as f:
        for line in f:
            s = line.strip()
            m = re.match(r"#\s*(ifdef|ifndef|else|endif|if)\b\s*(\S*)", s)
            if m:
                kw, arg = m.group(1), m.group(2)
                if kw == "ifdef":
                    stack.append(arg in defines)
                elif kw == "ifndef":
                    stack.append(arg not in defines)
                elif kw == "if":
                    stack.append(bool(re.match(r"\d+", arg) and int(arg) != 0))
                elif kw == "else":
                    stack[-1] = not stack[-1]
                elif kw == "endif":
                    stack.pop()
                continue
            if not (all(stack) and s) or s.startswith("#"):
                continue
            # skip C/C++ style comments (resources.txt has a header block)
            if s.startswith("//") or s.startswith("/*") or s.startswith("*"):
                continue
            # only real resource lines survive: <kind> <IDENT> "<file>"
            if not re.match(r'^\w+\s+[\w_]+\s+"[^"]+"\s*$', s):
                continue
            out.append(s)
    return out


def newer(target: str, *sources: str) -> bool:
    """True if target exists and is newer than all sources"""
    if not os.path.isfile(target):
        return False
    t = os.path.getmtime(target)
    return all(os.path.getmtime(s) <= t for s in sources if os.path.isfile(s))


def run(cmd: list[str]) -> None:
    subprocess.check_call(cmd)


def atomic_out(path: str):
    """return a temp path next to 'path'; caller renames when done.
    Keeps the extension (Inkscape ignores an export filename whose
    extension does not match the export type): foo.png -> foo.tmp.png"""
    base, ext = os.path.splitext(path)
    return base + ".tmp" + ext


class Tools:
    def __init__(self, a):
        self.inkscape = a.inkscape or shutil.which("inkscape") or "inkscape"
        self.convert = a.convert or shutil.which("magick") or shutil.which("convert") or "convert"
        self.xsltproc = a.xsltproc or shutil.which("xsltproc")
        self.src = a.src
        self.data = a.data
        self.magick7 = os.path.basename(self.convert).lower().startswith("magick")
        # branding config: @KEY@ replacements for the graphics SVGs
        self.config_vals: dict[str, str] = {}
        if getattr(a, "config", None):
            with open(a.config, encoding="utf-8") as f:
                for line in f:
                    line = line.strip()
                    if line and "=" in line and not line.startswith("#"):
                        k, v = line.split("=", 1)
                        self.config_vals[k.strip()] = v.strip()

    def resolve_svg(self, svg: str) -> str:
        """Return the SVG to render: if the file contains @KEY@ tokens of
        the branding config, write a replaced copy under <data>/graphics_pre
        and return that (mtime only bumps when the content changes, so the
        incremental checks keep working)."""
        if not self.config_vals:
            return svg
        try:
            txt = open(svg, encoding="utf-8").read()
        except (OSError, UnicodeDecodeError):
            return svg
        if "@" not in txt:
            return svg
        new = txt
        for k, v in self.config_vals.items():
            new = new.replace("@" + k + "@", v)
        if new == txt:
            return svg
        dst = os.path.join(self.data, "graphics_pre", os.path.basename(svg))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        if not (os.path.isfile(dst) and open(dst, encoding="utf-8").read() == new):
            with open(dst, "w", encoding="utf-8", newline="\n") as f:
                f.write(new)
        return dst

    def im(self, *args: str) -> list[str]:
        # ImageMagick 7's 'magick' takes the classic convert arguments
        # directly ('magick in.bmp ... out.png'); the 'convert' sub-command
        # is only accepted by some builds - IM 7.1 on Windows treats it as
        # an input file name ("no decode delegate for `convert'").
        return [self.convert] + list(args)

    # -- svg -> png ---------------------------------------------------------
    def svg_to_png(self, svg: str, png: str, *, dpi: float | None = None,
                   width: int | None = None, height: int | None = None,
                   white_bg: bool = False) -> None:
        os.makedirs(os.path.dirname(png), exist_ok=True)
        tmp = atomic_out(png)
        cmd = [self.inkscape, svg, "--export-type=png",
               "--export-overwrite", f"--export-filename={tmp}"]
        if dpi is not None:
            cmd.append(f"--export-dpi={dpi}")
        if width is not None:
            cmd.append(f"--export-width={width}")
        if height is not None:
            cmd.append(f"--export-height={height}")
        if white_bg:
            cmd += ["--export-background=white",
                    "--export-background-opacity=1.0"]
        run(cmd)
        os.replace(tmp, png)

    def noalias_svg(self, svg: str) -> str:
        """upstream renders icons from an xslt-preprocessed copy"""
        xsl = os.path.join(self.src, "build", "svg_preprocess.xsl")
        if not self.xsltproc or not os.path.isfile(xsl):
            return svg
        out = os.path.join(self.data, "icons_gl", os.path.basename(svg))
        if newer(out, svg, xsl):
            return out
        os.makedirs(os.path.dirname(out), exist_ok=True)
        # libxml/xsltproc on Windows chokes on backslash paths
        # ("I/O error : Invalid argument") - always pass forward slashes
        fwd = lambda p: p.replace("\\", "/")
        try:
            run([self.xsltproc, "--nonet", "--stringparam", "DisableAA_Select",
                 "MASK_NOAA_", "--output", fwd(out), fwd(xsl), fwd(svg)])
        except subprocess.CalledProcessError as e:
            # the xslt step only disables anti-aliasing on the mask parts;
            # rendering the raw svg is an acceptable fallback
            print(f"WARNING: xsltproc failed ({e.returncode}) for {svg} - "
                  "using the unprocessed svg", file=sys.stderr)
            if os.path.isfile(out):
                os.remove(out)
            return svg
        return out


# ---------------------------------------------------------------------------
# recipes
# ---------------------------------------------------------------------------

DISCLOSURE = ("location_pin", "notification_bell", "bluetooth",
              "warning_triangle", "rotate")


def make_graphic(t: Tools, name: str) -> str:
    """bitmap_graphic X "name" -> <data>/graphics2/name.png (returns path)"""
    out = os.path.join(t.data, "graphics2", name + ".png")
    gfx = os.path.join(t.src, "Data", "graphics")

    if name in DISCLOSURE:
        svg = os.path.join(gfx, name + ".svg")
        if not newer(out, svg):
            t.svg_to_png(svg, out, width=80, height=80)
        return out

    if name.startswith("gesture_"):
        svg = os.path.join(t.src, "doc", "manual", "figures", name + ".svg")
        if not newer(out, svg):
            t.svg_to_png(svg, out, width=82, height=82)
        return out

    # logo[_red]_{80,160,320}[_rgba], title[_red][_white]_{110,320,640}[_rgba]
    m = re.match(r"^(logo(?:_red)?|title(?:_red)?(?:_white)?)_(\d+)(_rgba)?$", name)
    if m:
        base, width, rgba = m.group(1), int(m.group(2)), bool(m.group(3))
        svg = t.resolve_svg(os.path.join(gfx, base + ".svg"))
        if not newer(out, svg):
            # the Win32 variants are 8-bit BMPs on white; the *_rgba ones
            # keep the alpha channel (upstream: graphics/ vs graphics2/)
            t.svg_to_png(svg, out, width=width, white_bg=not rgba)
        return out

    # launcher[_red]_640[_rgba]_{1,2}: render 640 wide, split in halves
    m = re.match(r"^(launcher(?:_red)?)_640(_rgba)?_([12])$", name)
    if m:
        base, rgba, half = m.group(1), bool(m.group(2)), m.group(3)
        svg = t.resolve_svg(os.path.join(gfx, base + ".svg"))
        full = os.path.join(t.data, "graphics2",
                            f"{base}_640{'_rgba' if rgba else ''}.png")
        if not newer(full, svg):
            t.svg_to_png(svg, full, width=640, white_bg=not rgba)
        if not newer(out, full):
            pattern = full[:-4] + "_%d.png"
            run(t.im(full, "-crop", "50%x100%", "+repage", "-scene", "1",
                     pattern))
        return out

    # dialog_title[_red], progress_border[_red], anything else: natural size
    svg = os.path.join(gfx, name + ".svg")
    if os.path.isfile(svg):
        svg = t.resolve_svg(svg)
        if not newer(out, svg):
            t.svg_to_png(svg, out, white_bg=True)
        return out

    sys.exit(f"ERROR: no recipe/source for bitmap_graphic '{name}'")


def make_icon(t: Tools, name: str, size: str) -> str:
    """bitmap_icon_scaled X "name" -> <data>/icons_gl/name_<size>.png"""
    out = os.path.join(t.data, "icons_gl", f"{name}_{size}.png")
    svg = os.path.join(t.src, "Data", "icons", name + ".svg")
    if not os.path.isfile(svg):
        sys.exit(f"ERROR: icon source missing: {svg}")
    src = t.noalias_svg(svg)
    if not newer(out, src):
        t.svg_to_png(src, out, dpi=96.0 * ICON_ZOOM[size])
    return out


def make_bitmap(t: Tools, name: str) -> str:
    """bitmap_bitmap X "name" -> <data>/bitmaps/name.png (grayscale)"""
    out = os.path.join(t.data, "bitmaps", name + ".png")
    bmp = os.path.join(t.src, "Data", "bitmaps", name + ".bmp")
    if not newer(out, bmp):
        os.makedirs(os.path.dirname(out), exist_ok=True)
        run(t.im(bmp, "+dither", "-type", "GrayScale",
                 "-define", "png:color-type=0", out))
    return out


def make_sound(t: Tools, name: str) -> str:
    """sound X "name" -> <data>/sound/name.raw: 16 bit, 44100 Hz, mono
    (what upstream produces with sox; linear resampling is plenty for
    the short beeps)"""
    out = os.path.join(t.data, "sound", name + ".raw")
    wav = os.path.join(t.src, "Data", "sound", name + ".wav")
    if newer(out, wav):
        return out
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with wave.open(wav, "rb") as w:
        nch, sw, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        frames = w.readframes(n)
    # -> list of mono int16 samples
    if sw == 1:
        samples = [(b - 128) << 8 for b in frames]
    elif sw == 2:
        samples = list(struct.unpack("<%dh" % (len(frames) // 2), frames))
    else:
        sys.exit(f"ERROR: unsupported sample width {sw} in {wav}")
    if nch > 1:
        samples = [sum(samples[i:i + nch]) // nch for i in range(0, len(samples), nch)]
    target = 44100
    if rate != target:
        ratio = rate / target
        count = int(len(samples) * target / rate)
        res = []
        for i in range(count):
            pos = i * ratio
            j = int(pos)
            frac = pos - j
            a = samples[min(j, len(samples) - 1)]
            b = samples[min(j + 1, len(samples) - 1)]
            res.append(int(a + (b - a) * frac))
        samples = res
    tmp = atomic_out(out)
    with open(tmp, "wb") as f:
        f.write(struct.pack("<%dh" % len(samples), *samples))
    os.replace(tmp, out)
    return out


# ---------------------------------------------------------------------------
# output
# ---------------------------------------------------------------------------

def write_c(path: str, blobs: list[tuple[str, str]]) -> None:
    """blobs: (symbol, file) -> resources.c with C byte arrays"""
    tmp = atomic_out(path)
    with open(tmp, "w", newline="\n") as f:
        f.write("/* generated by tools/python/gen_resources_data.py - do not edit */\n")
        f.write("#include <stddef.h>\n\n")
        for sym, file in blobs:
            data = open(file, "rb").read()
            f.write(f"/* {os.path.relpath(file)} */\n")
            f.write(f"const unsigned char {sym}[] = {{\n")
            for i in range(0, len(data), 16):
                f.write("  " + ",".join(f"0x{b:02x}" for b in data[i:i + 16]) + ",\n")
            if not data:
                f.write("  0\n")
            f.write("};\n")
            f.write(f"const size_t {sym}_size = {len(data)};\n")
            f.write(f"const unsigned char *const {sym}_end = {sym} + {len(data)};\n\n")
    os.replace(tmp, path)


def write_h(path: str, sounds: list[tuple[str, str]]) -> None:
    """named resource table (sounds), like tools/GenerateResources.pl"""
    tmp = atomic_out(path)
    with open(tmp, "w", newline="\n") as f:
        f.write("/* generated by tools/python/gen_resources_data.py - do not edit */\n")
        f.write("#include <cstddef>\n#include <span>\n\n")
        for name, file in sounds:
            f.write(f'extern "C" const std::byte resource_{name}[];\n')
            f.write(f'extern "C" const size_t resource_{name}_size;\n')
        f.write("\nstatic constexpr struct {\n  const char *name;\n"
                "  std::span<const std::byte> data;\n} named_resources[] = {\n")
        for name, file in sounds:
            f.write(f'  {{ "{name}", {{ resource_{name}, {os.path.getsize(file)} }} }},\n')
        f.write("  { 0, {} }\n};\n")
    os.replace(tmp, path)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--src", required=True, help="repository root")
    ap.add_argument("--data", required=True, help="data output dir")
    ap.add_argument("--resources", default=None, help="Data/resources.txt")
    ap.add_argument("--out-c", required=True)
    ap.add_argument("--out-h", required=True)
    ap.add_argument("--define", action="append", default=[])
    ap.add_argument("--inkscape")
    ap.add_argument("--convert")
    ap.add_argument("--xsltproc")
    ap.add_argument("--config", default=None,
                    help="branding config (KEY=VALUE per line); @KEY@ tokens "
                         "in graphics SVGs are replaced before rendering "
                         "(PROGRAM_NAME, PROGRAM_VERSION, ...)")
    a = ap.parse_args()

    a.src = os.path.abspath(a.src)
    a.data = os.path.abspath(a.data)
    resources = a.resources or os.path.join(a.src, "Data", "resources.txt")
    defines = set(a.define)
    t = Tools(a)

    blobs: list[tuple[str, str]] = []
    sounds: list[tuple[str, str]] = []
    for line in preprocess(resources, defines):
        m = re.match(r'^(\w+)\s+([\w_]+)\s+"([^"]+)"\s*$', line)
        if not m:
            sys.exit(f"ERROR: syntax error in resources.txt: {line}")
        kind, ident, name = m.groups()
        if kind == "bitmap_icon_scaled":
            blobs.append((f"resource_{ident}", make_icon(t, name, "96")))
            blobs.append((f"resource_{ident}_HD", make_icon(t, name, "160")))
            blobs.append((f"resource_{ident}_UHD", make_icon(t, name, "300")))
        elif kind == "bitmap_graphic":
            blobs.append((f"resource_{ident}", make_graphic(t, name)))
        elif kind == "bitmap_bitmap":
            blobs.append((f"resource_{ident}", make_bitmap(t, name)))
        elif kind == "sound":
            raw = make_sound(t, name)
            blobs.append((f"resource_{ident}", raw))
            sounds.append((ident, raw))
        elif kind in ("app_icon", "hatch_bitmap"):
            pass  # Win32 only
        else:
            sys.exit(f"ERROR: unknown resource kind '{kind}': {line}")

    os.makedirs(os.path.dirname(os.path.abspath(a.out_c)), exist_ok=True)
    os.makedirs(os.path.dirname(os.path.abspath(a.out_h)), exist_ok=True)
    write_c(a.out_c, blobs)
    write_h(a.out_h, sounds)
    print(f"resources: {len(blobs)} blobs ({len(sounds)} sounds) -> {a.out_c}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
