#!/usr/bin/env python3

import os, sys, glob, subprocess
from CMakeSoaringProject import create_xcsoar

# ====================
# The binary/output directory is FIXED by default
# (<project_dir>/Binaries/<project>/<toolchain>), so generated IDE
# project files keep working across branch switches.
#
# To separate output per branch again, set the environment variable
# XCSOAR_BRANCH - either to a fixed name (XCSOAR_BRANCH=work) or to the
# special value 'git', which resolves to the currently checked-out
# branch.
def current_git_branch():
    try:
        out = subprocess.check_output(
            ['git', 'rev-parse', '--abbrev-ref', 'HEAD'],
            text=True, stderr=subprocess.DEVNULL).strip()
        return '' if out == 'HEAD' else out  # detached HEAD -> no name
    except Exception:
        return ''

branch = os.environ.get('XCSOAR_BRANCH', '')
if branch == 'git':
    branch = current_git_branch()
branch = branch.replace('/', '_')  # topic/xyz -> topic_xyz (flat output dir)
# ====================


class ComputerDirectories(object):
    def __init__(self, name, directories):
        self.name = name
        self.project_dir = directories["project_dir"]
        self.third_party_dir = directories["third_party_dir"]
        self.binary_dir = directories["binary_dir"]
        self.program_dir = directories["program_dir"]
        self.link_libs = directories["link_libs"]

creation_flag = 15
if len(sys.argv) > 1:
  project = sys.argv[1]
else:
  project = "auto"
# 'auto'/'xcsoar' are placeholders - the effective app name is derived
# below from a <Name>.config in the repo root (branding), falling back
# to brand-neutral XCSoar

if len(sys.argv) > 2:
  toolchain = sys.argv[2]
else:
  toolchain = "msvc2026"


if len(sys.argv) > 3:
  creation_flag = sys.argv[3]


# A toolchain name may carry a build flavor ("msvc2026-OV"): the flavor
# names the build directory, the part in front names the compiler.
KNOWN_WINDOWS_TOOLCHAINS = [
    'mgw73', 'mgw103', 'mgw112', 'mgw122', 'mgw143', 'mgw152', 'ninja',
    'msvc2019', 'msvc2022', 'msvc2026',
    'clang10', 'clang11', 'clang12', 'clang13', 'clang14', 'clang15',
    'clang16', 'clang17', 'clang19', 'clang21',
]

if sys.platform.startswith('win'):
    base = toolchain[:-3] if toolchain.endswith('-OV') else toolchain
    if base not in KNOWN_WINDOWS_TOOLCHAINS:
        print('unknown toolchain "%s" - using mgw122 instead' % toolchain)
        print('known: %s (any of them with -OV for an OpenVario build)'
              % ', '.join(KNOWN_WINDOWS_TOOLCHAINS))
        toolchain = 'mgw122'
else:
    if not toolchain in ['unix', 'mingw']:
        # toolchain = 'unix'  # standard toolchain on Linux
        toolchain = 'mingw'  # standard toolchain on Linux

print('Project Name = ', project, 'toolchain = ', toolchain)

arguments = []
# project/app name: brand-neutral default 'XCSoar'; a <Name>.config in
# the repo root (see the branding topic: OpenSoar.config) switches the
# name - it decides the exe name (Step 4) and the Binaries subdir
app_name = 'XCSoar'
for _cfg in sorted(glob.glob('*.config')):
    _base = os.path.splitext(os.path.basename(_cfg))[0]
    if _base and _base[0].isupper():
        app_name = _base
        break
arguments.append(app_name)   # project_name
arguments.append(branch)        # branch ('' -> fixed dir 'build')

arguments.append(toolchain)     # build-toolchain
arguments.append(creation_flag)


print('Starting:', ' '.join(str(a) for a in arguments))
create_xcsoar(arguments)

