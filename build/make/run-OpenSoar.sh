#!/bin/bash

if [ ! -d "~/WinData" ]; then
  mkdir ~/WinData
fi 
echo $1 $0 $2
TARGET=$1
PROFILE=$2

if [ -z "$TARGET" ]; then TARGET=UNIX; fi
if [ -z "$PROFILE" ]; then PROFILE=August; fi

echo TARGET  = $TARGET
echo PROFILE = $PROFILE

OPENSOAR_CMD="./output/$TARGET/bin/OpenSoar"
case "$TARGET" in
      "WIN64" | "PC")  OPENSOAR_CMD+=".exe";;
      UNIX | OPENVARIO_CB2 | OPENVARIO)  ;;
esac


# OPENSOAR_CMD+=" -1500x880 -fly -datapath=/mnt/d/Data/OpenSoarData-WSL -profile=/mnt/d/Data/OpenSoarData-WSL/$PROFILE.prf"
OPENSOAR_CMD+=" -1500x880 -fly -datapath=../OpenSoarData -profile=../OpenSoarData/$PROFILE.prf"

echo $OPENSOAR_CMD
# ./output/PC/bin/OpenSoar.exe -1500x880 -fly -datapath= -profile=/home/august2111/WinData/default.prf > /home/august2111/WinData/Output-1.txt

$OPENSOAR_CMD  > /home/august2111/WinData/Output-1.txt