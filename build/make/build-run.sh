#!/bin/bash

# REMOVE_OLD=Yes
# löschen aller libraries:
REMOVE_LIBS=No
# das folgende löscht auch ALLE temporären (bitbake) Dateien ;-(
# damit wird der 'saubere' Ursprungszustand nach dem Clone hergestellt!
CLEANUP_ALL="No"

# ==== REMOTE ==========
### LOCAL=august-d
### GITHUB=august
### # REMOTE=august             # https://github.com/August2111/OpenSoar
### REMOTE=$LOCAL
REMOTE=OpenSoaring_OpenSoar     # LOCAL: /mnt/d/projects/OpenSoaring/OpenSoar
# REMOTE=opensoar                 # https://github.com/OpenSoaring/OpenSoar
# REMOTE=xcsoar

# ==== BRANCH ==========
BRANCH=dev-branch
# BRANCH=master

# BRANCH=xcsoar-master
# BRANCH=test-master
# BRANCH=test-opensoar   # variabler branch, um ein paar checkout tests zu machen (NICHT fixieren!)

GIT_RESET=
# 
GIT_RESET=$REMOTE/$BRANCH

# GIT_RESET=a64dde970934f24830bf61d38f5051f1fda668b5
# GIT_RESET=refs/tags/opensoar-7.39.19
# GIT_RESET=01479838c90375051ca833d6a22f97c3a3f18be2   # tag: opensoar-7.39.19
# GIT_RESET=8eaa73eb7050d15d328fd0ee79de997d1c9a5282  # tag: tag_for-branch-rebase (Error after thirdparty.mk)
# GIT_RESET=refs/tags/opensoar-7.39.19

CURR_DIR=$(pwd)

echo "CURR_DIR = $CURR_DIR"
#  change in 'OpenSoar' if you are not in really
if [ -d ../../OpenSoar ]; then
  cd ../..
fi

if [ "$CLEANUP_ALL" == "Yes" ]; then
  echo "git clean -xfd"
  git clean -xfd
fi

if [ -n "$GIT_RESET" ]; then
  echo "Resetting to $GIT_RESET"
  git fetch $REMOTE
  git reset --hard $GIT_RESET
  # git rev-parse HEAD
  # git describe
fi
## read -p "War jetzt ein Reset?"

source OpenSoar.config
OPENSOAR_VERSION=v$PROGRAM_VERSION
echo "OpenSoar-Version:  $OPENSOAR_VERSION"


# OPENSOAR_VERSION=v7.42.22.A

# /home/august2111/opt/android-sdk-linux/build-tools/33.0.2/apksigner

# targets=$1
### for ((i=0; i<8; ++i)); do
###     targets+=($$i)
###     echo "${targets[@]}"
### done

targets+=($1)

if [ "$targets" = "" ]; then 
targets=("WIN64" "PC" "ANDROIDFAT" "UNIX" "KOBO")
# targets=("PC" "ANDROIDFAT" "UNIX" "KOBO")
# targets=("UNIX")
# targets=("OPENVARIO_CB2")
# 
# targets=("OPENVARIO")
# targets=(WIN64  UNIX  KOBO)
# 
# targets=("WIN64")
# targets=("WIN64" "ANDROID")
# targets=("ANDROIDFAT")
# targets=("ANDROIDAARCH64")
# targets=("OPENVARIO ANDROIDAARCH64")

fi
## exit 

for target in "${targets[@]}"
do
    clear
    echo "Target = $target"
    # read -p "Stop" 
    ADD_ARGUMENT=""
    if [ "$REMOVE_OLD" = "Yes" ]; then (rm -vfr output/$target > output/$target-files.txt);fi
    case "$target" in
      "WIN64" | "PC")  ADD_ARGUMENT="everything check";;
      ## "ANDROID" | "ANDROIDAARCH64")  ADD_ARGUMENT="ANDROID_SDK=SDK NDK=r26d "
      ## ;;
      ## "ANDROID")  ADD_ARGUMENT="ANDROID_SDK=SDK NDK=r26d ";;
      ## "ANDROID_NDK=/home/august2111/opt/android-ndk-r26d" 
      "KOBO")  ADD_ARGUMENT="";;
      UNIX | OPENVARIO_CB2 | OPENVARIO)  ADD_ARGUMENT="everything check";;
    esac
    # ADD_ARGUMENT="CLANG=y LLVM=y $ADD_ARGUMENT"
    # ADD_ARGUMENT="CLANG=y $ADD_ARGUMENT"
    if [ "$REMOVE_LIBS" = "Yes" ]; then (rm -vf output/$target/lib/stamp);fi
    # make -j$(nproc) DEBUG=n TARGET=$target V=2
    # orig!  
    make -j$(nproc) DEBUG=n TARGET=$target USE_CCACHE=y V=2 $ADD_ARGUMENT
    # make -j1 DEBUG=n TARGET=$target USE_CCACHE=y V=2 $ADD_ARGUMENT
    echo "result = $?"
    echo "=========="
    # read -p "press key for next" > KEY
    
    DEPLOY_TARGET=
    case $target in
      WIN64) BIN_TARGET=output/$target/bin/OpenSoar.exe
             DEPLOY_DIR=Win64
      ;;             
      PC) BIN_TARGET=output/$target/bin/OpenSoar.exe
             DEPLOY_DIR=Win32
      ;;             
      KOBO) BIN_TARGET=output/$target/bin/*
             DEPLOY_DIR=Kobo
      ;;             
      ANDROID | ANDROIDAARCH64 | ANDROIDFAT) BIN_TARGET=output/ANDROID/bin/OpenSoar-debug.apk
             DEPLOY_DIR=Android
             DEPLOY_TARGET=OpenSoar-$PROGRAM_VERSION.apk  # OPENSOAR_VERSION w/0 v!
             # DEPLOY_TARGET=OpenSoar.apk
      ;;             
      UNIX )
             BIN_TARGET=output/UNIX/bin/OpenSoar
             DEPLOY_DIR=Linux
      ;;             
      OPENVARIO | OPENVARIO_CB2 )
             BIN_TARGET=output/OPENVARIO/bin/OpenSoar
             DEPLOY_DIR=OpenVario
      ;;             
    esac

    # echo "KEY = $KEY"
    # echo "=========="
    # if [ "$KEY" = "n" ]; then exit 0; fi
    # if [ ! "$KEY" = 27 ]; then exit 0; fi
    
    mkdir -p /mnt/d/OneDrive/OpenSoar/$OPENSOAR_VERSION/$DEPLOY_DIR/
    # copy -vfr output/$target/bin/OpenSoar*  /mnt/d/OneDrive/OpenSoar/$OPENSOAR_VERSION/$target/

#    rsync  -rc --progress $BIN_TARGET  /mnt/d/OneDrive/OpenSoar/$OPENSOAR_VERSION/$DEPLOY_TARGET
    rsync  -c --progress $BIN_TARGET  /mnt/d/OneDrive/OpenSoar/$OPENSOAR_VERSION/$DEPLOY_DIR/$DEPLOY_TARGET
    
    case "$target" in
      ANDROID | ANDROIDAARCH64 | ANDROIDFAT)
             /home/august2111/opt/android-sdk-linux/build-tools/33.0.2/apksigner sign --verbose --in $BIN_TARGET --out /mnt/d/OneDrive/OpenSoar/$OPENSOAR_VERSION/$DEPLOY_DIR/OpenSoar.apk -ks /mnt/d/android/keystore/OpenSoar.jks --ks-key-alias opensoar --ks-pass pass:LibeLLe7B    
      ;;    
      # UNIX) run-OpenSoar-UNIX.sh ;;             
    esac
done

echo "current commit:"
echo "==============="
git show --oneline -s
cd $CURR_DIR

if [ "${#targets[@]}" == "1" ]; then
for target in "${targets[@]}"
do
  echo "n in targets: ${#targets[@]}" 
  echo "target =  ${targets[0]}" 
  case ${targets[0]} in 
      OPENVARIO) 
                 echo "../run-OpenSoar.sh"
                 ../run-OpenSoar.sh ;;
      UNIX)
                 echo "../run-UNIX.sh"
                 ../run-OpenSoar-UNIX.sh ;;
  esac
  
done
fi

date


if [ 1 == 0 ]; then
apksigner verify --print-certs .\Github\ANDROIDAARCH64\bin\OpenSoar.apk

apksigner sign --verbose --in .\Github\ANDROIDAARCH64\bin\OpenSoar.apk --out .\Github\ANDROIDAARCH64\bin\OpenSoarSigned.apk -ks D:\android\keystore\OpenSoar.jks --ks-key-alias opensoar --ks-pass pass:LibeLLe7B    

apksigner verify --print-certs .\Github\ANDROIDAARCH64\bin\OpenSoarSigned.apk

fi

chmod +x  ./build/make/*