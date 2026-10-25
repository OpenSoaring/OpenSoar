#!/bin/bash
# Build the release packages with the make build: the four Android ABIs,
# Windows (32 and 64 bit), Linux and the Kobo.  Called by
# MakeComplete.sh, but works alone.
#
#   COMPLETE=y             rebuild from scratch (default; n keeps objects)
#   ANDROID_ARM64_ONLY=y   only the arm64 Android package (TARGETS=ANDROID64)
#   ANDROID_ONLY=y         only the Android packages of a full run
#                          (TARGETS="ANDROID ANDROID64")
#   TARGETS="..."          only these of ANDROID ANDROID64 ANDROID32
#                          ANDROID-X64 ANDROID-X86 PC WIN64 UNIX KOBO;
#                          ANDROID is the package with all four ABIs, the
#                          other ANDROID* targets one ABI each
#   JOBS=N                 parallel make jobs (default: number of CPUs)
#   PROGRAM_VERSION=x.y.z  version in the copied file names
#   PUBLISH_DIR=<folder>   where the packages are collected (default
#                          output/publish/<version>)
#
# The version comes from OpenSoar.config when the caller did not set it.
#
# Every package that is meant for the web page is also copied, under a
# name with the version, into $PUBLISH_DIR/<platform>/ (default
# output/publish/<version>/<platform>/):
#
#   Android/OpenSoar-<version>.apk         all four ABIs (TARGET=ANDROIDFAT)
#   Android/OpenSoar-<version>-arm64.apk   only arm64 (ANDROID64)
#   Android/OpenSoar-<version>-armv7.apk   only armeabi-v7a (ANDROID32)
#   Android/OpenSoar-<version>-x86_64.apk  only x86_64 (ANDROID-X64)
#   Android/OpenSoar-<version>-x86.apk     only x86 (ANDROID-X86)
#   Win32/OpenSoar-<version>-Win32.zip     PC (GDI) build
#   Win64/OpenSoar-<version>-Win64.zip     OpenGL build with the ANGLE DLLs
#   Unix/OpenSoar-<version>-<arch>.deb     installs with its dependencies
#   Kobo/KoboRoot.tgz                      the file a Kobo installs from
#   Kobo/OpenSoar-<version>-KoboRoot.tgz   the same, named by version
#
# A run replaces only the packages it builds, each one just before its
# build, so a failed build leaves no older package of that name behind,
# while the packages of the other targets stay where they are.  BUILT
# lists the packages of the last run, and publish_testing.py publishes
# only those.  Only a run that built all targets of ALL_TARGETS writes
# COMPLETE.

cd "$(dirname "$0")/../.."

COMPLETE=${COMPLETE:-y}
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 1)}
if [ -z "$PROGRAM_VERSION" ] && [ -f ./OpenSoar.config ]; then
  . ./OpenSoar.config
fi
VERSION_SUFFIX=${PROGRAM_VERSION:+-${PROGRAM_VERSION}}

# ALL_TARGETS is what a full run builds, the packages on the web page;
# KNOWN_TARGETS adds the single-ABI Android packages for testing, each
# about a quarter of the size of the one with all four ABIs.
ALL_TARGETS="ANDROID ANDROID64 PC WIN64 UNIX KOBO"
KNOWN_TARGETS="ANDROID ANDROID64 ANDROID32 ANDROID-X64 ANDROID-X86 PC WIN64 UNIX KOBO"
TARGETS=${TARGETS:-$ALL_TARGETS}
if [ "$ANDROID_ARM64_ONLY" == "y" ]; then
  TARGETS=ANDROID64
elif [ "$ANDROID_ONLY" == "y" ]; then
  TARGETS="ANDROID ANDROID64"
fi

# the names of the make targets work as well, in any case, separated
# by blanks or commas
normalized=
for t in ${TARGETS//,/ }; do
  t=$(echo "$t" | tr '[:lower:]' '[:upper:]')
  case "$t" in
    ANDROIDFAT) t=ANDROID ;;
    ANDROIDAARCH64|ANDROIDARM64) t=ANDROID64 ;;
    ANDROID7) t=ANDROID32 ;;
    ANDROIDX64) t=ANDROID-X64 ;;
    ANDROID86|ANDROIDX86) t=ANDROID-X86 ;;
  esac
  case " $KNOWN_TARGETS " in
    *" $t "*) ;;
    *) echo "Unknown target '$t' - known are: $KNOWN_TARGETS" >&2; exit 1 ;;
  esac
  normalized="$normalized $t"
done
TARGETS=${normalized# }

PUBLISH=${PUBLISH_DIR:-output/publish/${PROGRAM_VERSION:-unknown}}
# The make build signs the APK itself: with a release key
# (ANDROID_KEYSTORE, default ~/.android/mk.keystore, and ANDROID_KEY_ALIAS)
# it writes OpenSoar.apk, without one OpenSoar-debug.apk, signed with the
# debug key.  A debug-signed package cannot replace an installed
# release-signed one, so the run says which one it builds.
ANDROID_KEYSTORE=${ANDROID_KEYSTORE:-$HOME/.android/mk.keystore}
if [ -f "$ANDROID_KEYSTORE" ] && [ -n "$ANDROID_KEY_ALIAS" ]; then
  export ANDROID_KEYSTORE ANDROID_KEY_ALIAS
  APK=output/ANDROID/bin/OpenSoar.apk
  echo "Android: release-signed with $ANDROID_KEYSTORE ($ANDROID_KEY_ALIAS)"
else
  APK=output/ANDROID/bin/OpenSoar-debug.apk
  echo "Android: no release key (ANDROID_KEYSTORE/ANDROID_KEY_ALIAS), the APKs are debug-signed"
fi

wanted() {  # wanted <target>: true when this run builds it
  case " $TARGETS " in *" $1 "*) return 0 ;; esac
  return 1
}

publish() { # publish <platform> <file> [<name>]
  mkdir -p "$PUBLISH/$1"
  cp -v "$2" "$PUBLISH/$1/${3:-$(basename "$2")}"
  echo "$1/${3:-$(basename "$2")}" >> "$PUBLISH/BUILT"
}

unpublish() { # unpublish <platform> <name or pattern>, before its build
  rm -fv "$PUBLISH/$1/"$2
}

build() {   # build <target> <what it is called> [object directory] [make goals]
  echo "============================================================="
  echo "Make $2:"
  # (the object directories are paths without spaces, so $3 may name several)
  if [ "$COMPLETE" == "y" ] && [ -n "$3" ]; then rm -rf $3; fi
  # a failed target must stop the run: the steps after it would copy
  # the packages of an older build under the new version
  if ! make -j"$JOBS" DEBUG=n TARGET="$1" $4; then
    echo "Make $2 failed - stopping here." >&2
    exit 1
  fi
  echo "$2 ready!"
}

# only the marks of the last run start afresh; its packages stay until
# the build of the same target replaces them
mkdir -p "$PUBLISH"
rm -f "$PUBLISH/COMPLETE" "$PUBLISH/BUILT"

# Every Android build writes the same APK file, and a single-ABI build
# puts only its own ABI into it, so each package is copied away before
# the next build starts.
ANDROID_BUILT=   # object directories built from scratch in this run

android() {  # android <make target> <what> <object dirs> <name suffix>
  unpublish Android "OpenSoar${VERSION_SUFFIX}$4.apk"
  rm -f "$APK"
  build "$1" "$2" "$3" "$APK"
  ANDROID_BUILT="$ANDROID_BUILT $3"
  if [ ! -e "$APK" ]; then
    echo "'$APK' not available - stopping here." >&2
    exit 1
  fi
  cp -v "$APK" "output/ANDROID/bin/OpenSoar${VERSION_SUFFIX}$4.apk"
  publish Android "$APK" "OpenSoar${VERSION_SUFFIX}$4.apk"
}

if wanted ANDROID || wanted ANDROID64 || wanted ANDROID32 ||
   wanted ANDROID-X64 || wanted ANDROID-X86; then
  # a fresh run must not pick up yesterday's packages
  rm -rf output/ANDROID/opt
  rm -f "$APK"
fi

if wanted ANDROID64; then
  android ANDROIDAARCH64 "Android arm64" output/ANDROID/arm64-v8a/opt/src -arm64
fi
if wanted ANDROID32; then
  android ANDROID7 "Android armv7" output/ANDROID/armeabi-v7a/opt/src -armv7
fi
if wanted ANDROID-X64; then
  android ANDROIDX64 "Android x86_64" output/ANDROID/x86_64/opt/src -x86_64
fi
if wanted ANDROID-X86; then
  android ANDROID86 "Android x86" output/ANDROID/x86/opt/src -x86
fi

if wanted ANDROID; then
  # the ABIs that a single-ABI build of this run has just compiled from
  # scratch are not compiled once more
  FAT_DIRS=
  for d in output/ANDROID/armeabi-v7a/opt/src output/ANDROID/arm64-v8a/opt/src \
           output/ANDROID/x86/opt/src output/ANDROID/x86_64/opt/src; do
    case " $ANDROID_BUILT " in *" $d "*) ;; *) FAT_DIRS="$FAT_DIRS $d" ;; esac
  done
  android ANDROIDFAT "Android (all ABIs)" "${FAT_DIRS# }" ""
  ABIS=$(unzip -Z1 "$APK" 'lib/*' 2>/dev/null | cut -d/ -f2 | sort -u | wc -l)
  if [ "$ABIS" -lt 4 ]; then
    echo "'$APK' holds $ABIS ABIs instead of 4 - stopping here." >&2
    exit 1
  fi
fi

if wanted PC; then
  unpublish Win32 "OpenSoar${VERSION_SUFFIX}-Win32.zip"
  build PC "Win32" output/PC/opt/src "all dist"
  cp -v output/PC/bin/OpenSoar.exe "output/PC/bin/OpenSoar${VERSION_SUFFIX}.exe"
  publish Win32 "output/OpenSoar${VERSION_SUFFIX}-Win32.zip"
fi

if wanted WIN64; then
  unpublish Win64 "OpenSoar${VERSION_SUFFIX}-Win64.zip"
  build WIN64 "Win64" output/WIN64/opt/src "all dist"
  cp -v output/WIN64/bin/OpenSoar.exe "output/WIN64/bin/OpenSoar${VERSION_SUFFIX}.exe"
  publish Win64 "output/OpenSoar${VERSION_SUFFIX}-Win64.zip"
fi

if wanted UNIX; then
  unpublish Unix "OpenSoar${VERSION_SUFFIX}-*.deb"
  build UNIX "Linux" output/UNIX/opt/src "all deb"
  cp -v output/UNIX/bin/OpenSoar "output/UNIX/bin/OpenSoar${VERSION_SUFFIX}"
  publish Unix "$(ls -t output/OpenSoar${VERSION_SUFFIX}-*.deb | head -n 1)"
fi

if wanted KOBO; then
  unpublish Kobo KoboRoot.tgz
  unpublish Kobo "OpenSoar${VERSION_SUFFIX}-KoboRoot.tgz"
  build KOBO "Kobo" output/KOBO/opt/src output/KOBO/KoboRoot.tgz
  publish Kobo output/KOBO/KoboRoot.tgz
  publish Kobo output/KOBO/KoboRoot.tgz "OpenSoar${VERSION_SUFFIX}-KoboRoot.tgz"
fi

complete=y
for t in $ALL_TARGETS; do
  wanted "$t" || complete=n
done
if [ "$complete" == "y" ]; then
  {
    echo "version=$PROGRAM_VERSION"
    echo "commit=$(git rev-parse HEAD 2>/dev/null)"
    echo "date=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "targets=$TARGETS"
  } > "$PUBLISH/COMPLETE"
  echo "All targets built - $PUBLISH is ready for publish_testing.py."
else
  echo "Only $TARGETS built - $PUBLISH is not marked complete."
fi
