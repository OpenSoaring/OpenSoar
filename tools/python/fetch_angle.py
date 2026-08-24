#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright The XCSoar Project
"""
Fetch the prebuilt ANGLE libraries (OpenGL ES 2/3 via D3D11) for Windows
from https://github.com/XCSoar/angle-libs and create import libraries.

This is the Python/Windows counterpart of windows/fetch-angle-from-github.sh
(which needs bash + mingw-w64 dlltool and is used by the upstream
cross-build on Linux).  It works with MSVC *and* MinGW:

  * download  angle-windows-<arch>-<commit>.tar.gz  (sha256 verified, cached)
  * unpack    include/  (GLES2, GLES3, EGL, KHR)  and  bin/  (libEGL.dll,
              libGLESv2.dll)
  * create    import libraries in lib/:
              - MSVC : libEGL.lib / libGLESv2.lib      (dumpbin + lib.exe)
              - MinGW: libEGL.dll.a / libGLESv2.dll.a  (gendef + dlltool)

Usage:
  fetch_angle.py <output_dir> [--arch x64|x86] [--toolset msvc|mingw|auto]
                 [--download-dir DIR]

Typical (called by CMake):
  fetch_angle.py C:/Projects/link_libs/angle --toolset msvc
"""

from __future__ import annotations

import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request

# keep in sync with windows/fetch-angle-from-github.sh
ANGLE_LIBS_TAG = "a96fca8"
ANGLE_LIBS_COMMIT = "a96fca8d5ee2ca61e8de419e38cd577579281c9e"
SHA256 = {
    "x64": "82723e19795d683e6af2afadf39fb00d248d6a5a2cb2af9faeebc017a7f4f5d8",
    "x86": "02eb72f10673bca9569c0c044f5ff877a2815a85b9c47555ec1b04a5d36a4c00",
}
DLLS = ("libEGL.dll", "libGLESv2.dll")


def tarball_name(arch: str) -> str:
    return f"angle-windows-{arch}-{ANGLE_LIBS_COMMIT}.tar.gz"


def tarball_url(arch: str) -> str:
    return (f"https://github.com/XCSoar/angle-libs/releases/download/"
            f"{ANGLE_LIBS_TAG}/{tarball_name(arch)}")


def sha256_of(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def download(arch: str, download_dir: str) -> str:
    os.makedirs(download_dir, exist_ok=True)
    dest = os.path.join(download_dir, tarball_name(arch))
    if os.path.isfile(dest):
        if sha256_of(dest) == SHA256[arch]:
            print(f"Using cached {tarball_name(arch)}")
            return dest
        print("WARNING: cached tarball failed verification, re-downloading")
        os.remove(dest)
    print(f"Downloading {tarball_url(arch)}")
    tmp = dest + ".part"
    with urllib.request.urlopen(tarball_url(arch)) as r, open(tmp, "wb") as f:
        shutil.copyfileobj(r, f)
    if sha256_of(tmp) != SHA256[arch]:
        os.remove(tmp)
        sys.exit("ERROR: checksum verification failed")
    os.replace(tmp, dest)
    return dest


def extract(tarball: str, arch: str, out_dir: str, tmp_dir: str) -> None:
    with tarfile.open(tarball, "r:gz") as tf:
        tf.extractall(tmp_dir)
    src = os.path.join(tmp_dir, f"angle-windows-{arch}-{ANGLE_LIBS_COMMIT}")
    bin_dir = os.path.join(out_dir, "bin")
    inc_dir = os.path.join(out_dir, "include")
    os.makedirs(bin_dir, exist_ok=True)
    for dll in DLLS:
        shutil.copy2(os.path.join(src, dll), bin_dir)
    src_inc = os.path.join(src, "include")
    if os.path.isdir(src_inc):
        if os.path.isdir(inc_dir):
            shutil.rmtree(inc_dir)
        shutil.copytree(src_inc, inc_dir)
    print(f"  DLLs    -> {bin_dir}")
    print(f"  headers -> {inc_dir}")


# ---------------------------------------------------------------------------
# import libraries
# ---------------------------------------------------------------------------

def find_tool(name: str) -> str | None:
    return shutil.which(name)


def find_msvc_tool(name: str) -> str | None:
    """dumpbin/lib: on PATH inside a VS developer prompt, otherwise look
    through the VS installation via vswhere."""
    p = find_tool(name)
    if p:
        return p
    pf86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    vswhere = os.path.join(pf86, "Microsoft Visual Studio", "Installer",
                           "vswhere.exe")
    if not os.path.isfile(vswhere):
        return None
    try:
        root = subprocess.check_output(
            [vswhere, "-latest", "-products", "*",
             "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
             "-property", "installationPath"], text=True).strip()
    except subprocess.CalledProcessError:
        return None
    tools = os.path.join(root, "VC", "Tools", "MSVC")
    if not os.path.isdir(tools):
        return None
    versions = sorted(os.listdir(tools))
    if not versions:
        return None
    cand = os.path.join(tools, versions[-1], "bin", "Hostx64", "x64",
                        name + ".exe")
    return cand if os.path.isfile(cand) else None


def exports_from_dumpbin(dumpbin: str, dll: str) -> list[str]:
    out = subprocess.check_output([dumpbin, "/nologo", "/exports", dll],
                                  text=True, errors="replace")
    names: list[str] = []
    in_table = False
    for line in out.splitlines():
        if re.match(r"\s*ordinal\s+hint\s+RVA\s+name", line):
            in_table = True
            continue
        if in_table:
            if line.strip().startswith("Summary"):
                break
            m = re.match(r"\s*\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(\S+)", line)
            if m:
                names.append(m.group(1))
    return names


def make_import_libs_msvc(out_dir: str, arch: str, tmp_dir: str) -> None:
    dumpbin = find_msvc_tool("dumpbin")
    lib_exe = find_msvc_tool("lib")
    if not dumpbin or not lib_exe:
        sys.exit("ERROR: dumpbin.exe / lib.exe not found - run from a "
                 "Visual Studio developer prompt or install the VC++ tools")
    lib_dir = os.path.join(out_dir, "lib")
    os.makedirs(lib_dir, exist_ok=True)
    machine = "x64" if arch == "x64" else "x86"
    for dll in DLLS:
        dll_path = os.path.join(out_dir, "bin", dll)
        base = dll[:-4]
        names = exports_from_dumpbin(dumpbin, dll_path)
        if not names:
            sys.exit(f"ERROR: no exports found in {dll}")
        def_file = os.path.join(tmp_dir, base + ".def")
        with open(def_file, "w") as f:
            f.write(f"LIBRARY {dll}\nEXPORTS\n")
            for n in names:
                f.write(f"  {n}\n")
        lib_file = os.path.join(lib_dir, base + ".lib")
        subprocess.check_call([lib_exe, "/nologo", f"/def:{def_file}",
                               f"/machine:{machine}", f"/out:{lib_file}"])
        print(f"  {os.path.basename(lib_file)}: {len(names)} exports")


def make_import_libs_mingw(out_dir: str, arch: str, tmp_dir: str) -> None:
    gendef = find_tool("gendef")
    dlltool = (find_tool("x86_64-w64-mingw32-dlltool" if arch == "x64"
                         else "i686-w64-mingw32-dlltool")
               or find_tool("dlltool"))
    if not gendef or not dlltool:
        sys.exit("ERROR: gendef / dlltool not found - install mingw-w64-tools")
    lib_dir = os.path.join(out_dir, "lib")
    os.makedirs(lib_dir, exist_ok=True)
    for dll in DLLS:
        dll_path = os.path.abspath(os.path.join(out_dir, "bin", dll))
        base = dll[:-4]
        subprocess.check_call([gendef, dll_path], cwd=tmp_dir,
                              stdout=subprocess.DEVNULL,
                              stderr=subprocess.DEVNULL)
        def_file = os.path.join(tmp_dir, base + ".def")
        if arch == "x86":
            # stdcall @N decorations, see fetch-angle-from-github.sh
            with open(def_file) as f:
                lines = f.read().splitlines()
            with open(def_file, "w") as f:
                for l in lines:
                    if re.match(r"^(gl|EGL_|egl)[A-Za-z0-9_]*$", l):
                        l += "@0"
                    f.write(l + "\n")
        lib_file = os.path.join(lib_dir, base + ".dll.a")
        subprocess.check_call([dlltool, "-d", def_file, "-D", dll,
                               "-l", lib_file])
        print(f"  {os.path.basename(lib_file)}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("output_dir")
    ap.add_argument("--arch", choices=("x64", "x86"), default="x64")
    ap.add_argument("--toolset", choices=("msvc", "mingw", "auto"),
                    default="auto")
    ap.add_argument("--download-dir", default=None)
    args = ap.parse_args()

    out_dir = os.path.abspath(args.output_dir)
    download_dir = args.download_dir or os.path.join(out_dir, "download")
    toolset = args.toolset
    if toolset == "auto":
        toolset = "msvc" if (find_msvc_tool("lib") and
                             os.name == "nt") else "mingw"

    print(f"ANGLE {ANGLE_LIBS_TAG} (windows-{args.arch}) -> {out_dir} "
          f"[{toolset}]")
    tarball = download(args.arch, download_dir)
    with tempfile.TemporaryDirectory() as tmp:
        extract(tarball, args.arch, out_dir, tmp)
        if toolset == "msvc":
            make_import_libs_msvc(out_dir, args.arch, tmp)
        else:
            make_import_libs_mingw(out_dir, args.arch, tmp)
    with open(os.path.join(out_dir, ".stamp"), "w") as f:
        f.write(f"{ANGLE_LIBS_TAG} {args.arch} {toolset}\n")
    print("done.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
