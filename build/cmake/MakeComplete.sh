#!/bin/bash
# Release driver for a build machine: bring the working tree to the
# state of a remote branch, then build everything (MakeAll.sh).
#
#   GIT_REMOTE=OpenSoaring  GIT_BRANCH=master  ./build/cmake/MakeComplete.sh
#
#   GIT_REMOTE   remote to fetch from      (default: OpenSoaring)
#   GIT_BRANCH   branch to build           (default: master)
#   COMPILE=n    only update, do not build
#   FORCE=y      reset even though the working tree has local changes
#
# The reset is a HARD one - that is the point on a build machine, but it
# discards local work, so a dirty tree stops the script unless FORCE=y.

set -e
cd "$(dirname "$0")/../.."

GIT_REMOTE=${GIT_REMOTE:-${GIT_REPOSITORY:-OpenSoaring}}
GIT_BRANCH=${GIT_BRANCH:-master}
COMPILE=${COMPILE:-y}
export COMPLETE=${COMPLETE:-y}

echo "Make Complete: ${GIT_REMOTE}/${GIT_BRANCH}"
echo "================================================"

if [ -n "$(git status --porcelain)" ] && [ "$FORCE" != "y" ]; then
  echo "The working tree has local changes, and this script would throw" >&2
  echo "them away with 'git reset --hard'.  Commit or stash them, or" >&2
  echo "run again with FORCE=y if that is what you want." >&2
  git status --short >&2
  exit 1
fi

git fetch "${GIT_REMOTE}"
git reset --hard "${GIT_REMOTE}/${GIT_BRANCH}"
git submodule update --init --recursive

if [ -f ./OpenSoar.config ]; then . ./OpenSoar.config; fi
echo "PROGRAM_VERSION = ${PROGRAM_VERSION}"
export PROGRAM_VERSION

chmod +x ./build/cmake/*.sh

if [ "$COMPILE" == "y" ]; then ./build/cmake/MakeAll.sh; fi
