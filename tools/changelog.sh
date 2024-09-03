#!/bin/bash

TAG="${1}"
VERSION=$(echo "$TAG" | cut -f2 -d v)
STARTLINENUMBER=$(grep -n -E "^Version $VERSION " NEWS.txt | cut -f1 -d:)
if [ -z "${STARTLINENUMBER}" ]; then
  echo "ERROR: Version $1 not found in OpenSoar-News.md"
  exit 1
fi
FINLINENUMBER=$(tail -n +"${STARTLINENUMBER}" OpenSoar-News.md | grep -E '^$' -n | head -n 1| cut -f1 -d:)
FINLINENUMBER=$(( "${STARTLINENUMBER}" + "${FINLINENUMBER}" - 2 ))
sed -n "${STARTLINENUMBER}","${FINLINENUMBER}"p OpenSoar-News.md
