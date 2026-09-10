#!/bin/bash
# Build the release packages with the make build: the four Android ABIs,
# then Windows and Linux.  Called by MakeComplete.sh, but works alone.
#
#   COMPLETE=y             rebuild from scratch (default; n keeps objects)
#   ANDROID_ARM64_ONLY=y   only the 64 bit Android package
#   ANDROID_ONLY=y         only the Android packages
#   PROGRAM_VERSION=x.y.z  version in the copied file names
#
# The version comes from OpenSoar.config when the caller did not set it.

cd "$(dirname "$0")/../.."

COMPLETE=${COMPLETE:-y}
if [ -z "$PROGRAM_VERSION" ] && [ -f ./OpenSoar.config ]; then
  . ./OpenSoar.config
fi
VERSION_SUFFIX=${PROGRAM_VERSION:+-${PROGRAM_VERSION}}

APK=output/ANDROID/bin/OpenSoar-unsigned.apk

# a fresh run must not pick up yesterday's packages
rm -rf output/ANDROID/opt
rm -f "$APK"

build() {   # build <target> <what it is called> [object directory]
  echo "============================================================="
  echo "Make $2:"
  if [ "$COMPLETE" == "y" ] && [ -n "$3" ]; then rm -rf "$3"; fi
  make DEBUG=n TARGET="$1"
  echo "$2 ready!"
}

build ANDROIDAARCH64 "Android v8a (64 bit)" output/ANDROID/arm64-v8a/opt/src

if [ ! -e "$APK" ]; then
  echo "'$APK' not available - stopping here." >&2
  exit 1
fi

cp -v "$APK" "output/ANDROID/bin/OpenSoar${VERSION_SUFFIX}-64.apk"

if [ "$ANDROID_ARM64_ONLY" == "y" ]; then
  echo "ANDROID_ARM64_ONLY = y - done."
  exit 0
fi

build ANDROID     "Android v7a (32 bit)" output/ANDROID/armeabi-v7a/opt/src
build ANDROIDX64  "Android x86_64"       output/ANDROID/x86_64/opt/src
build ANDROID86   "Android x86"          output/ANDROID/x86/opt/src

cp -v "$APK" "output/ANDROID/bin/OpenSoar${VERSION_SUFFIX}.apk"

if [ "$ANDROID_ONLY" == "y" ]; then
  echo "ANDROID_ONLY = y - done."
  exit 0
fi

build WIN64 "Win64" output/WIN64/opt/src
cp -v output/WIN64/bin/OpenSoar.exe "output/WIN64/bin/OpenSoar${VERSION_SUFFIX}.exe"

build UNIX "Linux" output/UNIX/opt/src
cp -v output/UNIX/bin/OpenSoar "output/UNIX/bin/OpenSoar${VERSION_SUFFIX}"
