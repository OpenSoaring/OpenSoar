#!/bin/bash

### REMOTE=$LOCAL
### BRANCH=dev-branch

CURR_DIR=$(pwd)

echo "CURR_DIR = $CURR_DIR"
cd ../..

# das folgende löscht auch ALLE temporären (bitbake) Dateien ;-(
# damit wird der 'saubere' Ursprungszustand nach dem Clone hergestellt!
CLEANUP_ALL="n"
if [ "$CLEANUP_ALL" == "y" ]; then
  echo "git clean -xfd"
  git clean -xfd
fi

echo "git submodule foreach --recursive git clean -xfd"
git submodule foreach --recursive git clean -xfd
echo "git reset --hard"
git reset --hard
echo "submodule foreach --recursive git reset --hard"
git submodule foreach --recursive git reset --hard
echo "git submodule update --init --recursive"
git submodule update --init --recursive

chmod +x  ./build/make/*
