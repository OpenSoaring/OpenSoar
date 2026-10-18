#!/bin/bash
# Build the release packages with the make build: the four Android ABIs,
# Windows (32 and 64 bit), Linux and the Kobo.  Called by
# MakeComplete.sh, but works alone.
#
#   COMPLETE=y             rebuild from scratch (default; n keeps objects)
#   ANDROID_ARM64_ONLY=y   only the 64 bit Android package
#   ANDROID_ONLY=y         only the Android packages
#   TARGETS="..."          only these of ANDROID PC WIN64 UNIX KOBO
#   JOBS=N                 parallel make jobs (default: number of CPUs)
#   PROGRAM_VERSION=x.y.z  version in the copied file names
#
# The version comes from OpenSoar.config when the caller did not set it.
#
# Every package that is meant for the web page is also copied, under a
# name with the version, into output/publish/<version>/<platform>/:
#
#   Android/OpenSoar-<version>-arm64.apk   only arm64
#   Android/OpenSoar-<version>.apk         all four ABIs (TARGET=ANDROIDFAT)
#   Win32/OpenSoar-<version>-Win32.zip     PC (GDI) build
#   Win64/OpenSoar-<version>-Win64.zip     OpenGL build with the ANGLE DLLs
#   Unix/OpenSoar-<version>-<arch>.deb     installs with its dependencies
#   Kobo/KoboRoot.tgz                      the file a Kobo installs from
#   Kobo/OpenSoar-<version>-KoboRoot.tgz   the same, named by version
#
# Only a run that built all targets writes output/publish/<version>/COMPLETE.
# The upload (publish_testing.py) refuses a folder without it, so that a
# partial run, which leaves packages of an older build in place, never
# reaches the web page.

cd "$(dirname "$0")/../.."

COMPLETE=${COMPLETE:-y}
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 1)}
if [ -z "$PROGRAM_VERSION" ] && [ -f ./OpenSoar.config ]; then
  . ./OpenSoar.config
fi
VERSION_SUFFIX=${PROGRAM_VERSION:+-${PROGRAM_VERSION}}

ALL_TARGETS="ANDROID PC WIN64 UNIX KOBO"
TARGETS=${TARGETS:-$ALL_TARGETS}
if [ "$ANDROID_ONLY" == "y" ] || [ "$ANDROID_ARM64_ONLY" == "y" ]; then
  TARGETS=ANDROID
fi

PUBLISH=output/publish/${PROGRAM_VERSION:-unknown}
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

# packages of an earlier run of the same version must not survive
rm -rf "$PUBLISH"

if wanted ANDROID; then
  # a fresh run must not pick up yesterday's packages
  rm -rf output/ANDROID/opt
  rm -f "$APK"

  # one build per package: every Android build writes the same APK file,
  # and a single-ABI build puts only its own ABI into it
  build ANDROIDAARCH64 "Android arm64" output/ANDROID/arm64-v8a/opt/src "$APK"
  if [ ! -e "$APK" ]; then
    echo "'$APK' not available - stopping here." >&2
    exit 1
  fi
  cp -v "$APK" "output/ANDROID/bin/OpenSoar${VERSION_SUFFIX}-arm64.apk"
  publish Android "$APK" "OpenSoar${VERSION_SUFFIX}-arm64.apk"

  if [ "$ANDROID_ARM64_ONLY" == "y" ]; then
    echo "ANDROID_ARM64_ONLY = y - done."
    exit 0
  fi

  rm -f "$APK"
  build ANDROIDFAT "Android (all ABIs)" \
    "output/ANDROID/armeabi-v7a/opt/src output/ANDROID/x86/opt/src output/ANDROID/x86_64/opt/src" \
    "$APK"
  ABIS=$(unzip -Z1 "$APK" 'lib/*' 2>/dev/null | cut -d/ -f2 | sort -u | wc -l)
  if [ "$ABIS" -lt 4 ]; then
    echo "'$APK' holds $ABIS ABIs instead of 4 - stopping here." >&2
    exit 1
  fi
  cp -v "$APK" "output/ANDROID/bin/OpenSoar${VERSION_SUFFIX}.apk"
  publish Android "$APK" "OpenSoar${VERSION_SUFFIX}.apk"

  if [ "$ANDROID_ONLY" == "y" ]; then
    echo "ANDROID_ONLY = y - done."
    exit 0
  fi
fi

if wanted PC; then
  build PC "Win32" output/PC/opt/src "all dist"
  cp -v output/PC/bin/OpenSoar.exe "output/PC/bin/OpenSoar${VERSION_SUFFIX}.exe"
  publish Win32 "output/OpenSoar${VERSION_SUFFIX}-Win32.zip"
fi

if wanted WIN64; then
  build WIN64 "Win64" output/WIN64/opt/src "all dist"
  cp -v output/WIN64/bin/OpenSoar.exe "output/WIN64/bin/OpenSoar${VERSION_SUFFIX}.exe"
  publish Win64 "output/OpenSoar${VERSION_SUFFIX}-Win64.zip"
fi

if wanted UNIX; then
  build UNIX "Linux" output/UNIX/opt/src "all deb"
  cp -v output/UNIX/bin/OpenSoar "output/UNIX/bin/OpenSoar${VERSION_SUFFIX}"
  publish Unix "$(ls -t output/OpenSoar${VERSION_SUFFIX}-*.deb | head -n 1)"
fi

if wanted KOBO; then
  build KOBO "Kobo" output/KOBO/opt/src output/KOBO/KoboRoot.tgz
  publish Kobo output/KOBO/KoboRoot.tgz
  publish Kobo output/KOBO/KoboRoot.tgz "OpenSoar${VERSION_SUFFIX}-KoboRoot.tgz"
fi

if [ "$TARGETS" == "$ALL_TARGETS" ]; then
  {
    echo "version=$PROGRAM_VERSION"
    echo "commit=$(git rev-parse HEAD 2>/dev/null)"
    echo "date=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "targets=$ALL_TARGETS"
  } > "$PUBLISH/COMPLETE"
  echo "All targets built - $PUBLISH is ready for publish_testing.py."
else
  echo "Only $TARGETS built - $PUBLISH is not marked complete."
fi
