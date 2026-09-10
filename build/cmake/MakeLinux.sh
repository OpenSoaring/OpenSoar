#!/bin/bash
# Build the Linux version with the make build and run it.
#
#   MakeLinux.sh [options for the program]
#
# Nothing is fetched and nothing is reset here - the working tree is
# built as it is.  (This script used to reset the repository to a fixed
# remote branch, which quietly threw away local work.)

set -e
cd "$(dirname "$0")/../.."

make TARGET=UNIX DEBUG=n
exec output/UNIX/bin/OpenSoar "${@:--fly}"
