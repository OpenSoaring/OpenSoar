#!/bin/bash
# Build all four Android ABIs with the make build.

set -e
cd "$(dirname "$0")/../.."

for target in ANDROID ANDROIDAARCH64 ANDROIDX64 ANDROID86; do
  echo "=== $target"
  make DEBUG=n TARGET="$target"
done
