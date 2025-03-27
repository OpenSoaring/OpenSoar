#!/usr/bin/env -S python3 -u

import os, os.path
import re
import sys

if len(sys.argv) != 13  or True :
    print("args    : ", len(sys.argv)) 
    print("                 : ", sys.argv[0] ) 
    print("arg LIB_PATH     : ", sys.argv[1] ) 
    print("arg HOST_TRIPLET : ", sys.argv[2] )
    print("arg ARCH_CFLAGS  : ", sys.argv[3] )
    print("arg CPPFLAGS     : ", sys.argv[4] )
    print("arg ARCH_LDFLAGS : ", sys.argv[5] )
    print("arg CC           : ", sys.argv[6] )
    print("arg CXX          : ", sys.argv[7] )
    print("arg AR           : ", sys.argv[8] )
    print("arg ARFLAGS      : ", sys.argv[9] )
    print("arg RANLIB       : ", sys.argv[10] )
    print("arg STRIP        : ", sys.argv[11] )
    print("arg WINDRES      : ", sys.argv[12] )

if len(sys.argv) != 13:
    print("Usage: build.py LIB_PATH HOST_TRIPLET ARCH_CFLAGS CPPFLAGS ARCH_LDFLAGS CC CXX AR ARFLAGS RANLIB STRIP WINDRES", file=sys.stderr)
    sys.exit(1)

lib_path, host_triplet, arch_cflags, cppflags, arch_ldflags, cc, cxx, ar, arflags, ranlib, strip, windres = sys.argv[1:]

# the path to the XCSoar sources
xcsoar_path = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]) or '.', '..'))
sys.path[0] = os.path.join(xcsoar_path, 'build/python')

# output directories
from build.dirs import tarball_path, src_path

lib_path = os.path.abspath(lib_path)
build_path = os.path.join(lib_path, 'build')
install_prefix = os.path.join(lib_path, host_triplet)

if 'MAKEFLAGS' in os.environ:
    # build/make.mk adds "--no-builtin-rules --no-builtin-variables",
    # which breaks the zlib Makefile (and maybe others)
    del os.environ['MAKEFLAGS']

from build.toolchain import Toolchain, NativeToolchain
toolchain = Toolchain(xcsoar_path, lib_path,
                      tarball_path, src_path, build_path, install_prefix,
                      host_triplet,
                      arch_cflags, cppflags, arch_ldflags, cc, cxx, ar, arflags,
                      ranlib, strip, windres)

# a list of third-party libraries to be used by XCSoar
from build.libs import *

with_geotiff = False
with_skysight = False

thirdparty_libs = [
    zlib,
    libfmt,
    libsodium,
    openssl,
    cares,
    curl,
    lua
]

if toolchain.is_windows:
    # with_geotiff = True
    thirdparty_libs.remove(openssl)

    # Some libraries (such as CURL) want to use the min()/max() macros
    toolchain.cppflags = cppflags.replace('-DNOMINMAX', '')

    # Explicitly disable _FORTIFY_SOURCE because it is broken with
    # mingw.  This prevents some libraries such as libsodium to enable
    # it.
    toolchain.cppflags += ' -D_FORTIFY_SOURCE=0'
    with_skysight = True
elif toolchain.is_target_ios:
    print('toolchain: iOS')
    thirdparty_libs.append(sdl2)
elif toolchain.is_darwin:  # and not iOS!
    print('toolchain: MacOS')
    thirdparty_libs = []
elif toolchain.is_android:
    with_skysight = True
    thirdparty_libs.remove(zlib)
    ## thirdparty_libs += [
    ## ]
elif toolchain.is_kobo: # '-kobo-linux-' in host_triplet:
    thirdparty_libs = [
        binutils,
        linux_headers,
        gcc_bootstrap,
        musl,
        gcc
    ] + thirdparty_libs
    thirdparty_libs += [
        freetype,
        libpng,
        libjpeg,
        libsalsa,
        libusb,
        simple_usbmodeswitch
    ]
elif toolchain.is_unix:
    print('toolchain: UNIX')

    thirdparty_libs = []
    thirdparty_libs += [
      netcdf,
      netcdfcxx
    ]
else:
    print('toolchain: ', toolchain.build_path, ' - ', toolchain.install_prefix, ' - ', toolchain.host_triplet)
    raise RuntimeError('Unrecognized target')

# do it before(!) 'with_skysight'
if with_geotiff or with_skysight: 
    thirdparty_libs += [
        sqlite3,
        proj,
        libtiff,
        libgeotiff
    ]

if with_skysight:
    # with_geotiff = True - the geotiff libs have to be included before skysight
    # therefore 
    thirdparty_libs += [
        netcdf,
        netcdfcxx
    ]
    
# build the third-party libraries
for x in thirdparty_libs:
    if not x.is_installed(toolchain):
        x.build(toolchain)
