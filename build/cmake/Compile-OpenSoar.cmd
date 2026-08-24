@echo off
setlocal
cd /D %~dp0../..

::  Compile-OpenSoar.cmd  [toolchain]  [steps]
::
::  toolchain   msvc2022, msvc2026      Visual Studio (OpenGL via ANGLE + SDL2)
::              msvc2026-OV            same, with the OpenVario menus - own
::                                     build directory and own solution
::              mgw112, mgw122         MinGW
::              clang15                Clang
::              (default: msvc2026)
::
::  steps       bit mask, add up what shall happen (default: 15 = all)
::                1  configure (CMake, builds the third-party libraries
::                   on the first run - that takes a while)
::                2  build
::                4  install
::                8  run
::              so 1 = configure only, 3 = configure and build, 15 = everything
::
::  Examples:   Compile-OpenSoar.cmd
::              Compile-OpenSoar.cmd msvc2026 1
::              Compile-OpenSoar.cmd msvc2026-OV 3

if /I "%~1" == "-h"     goto :usage
if /I "%~1" == "--help" goto :usage
if /I "%~1" == "/?"     goto :usage

set XCSOAR_TOOLCHAIN=%~1
if not defined XCSOAR_TOOLCHAIN set XCSOAR_TOOLCHAIN=msvc2026
set COMPILE_PARTS=%~2
if not defined COMPILE_PARTS set COMPILE_PARTS=15

echo Source     = %CD%
echo Toolchain  = %XCSOAR_TOOLCHAIN%
echo Steps      = %COMPILE_PARTS%
echo.

PATH=%CD%;%CD%\build\cmake\python;%PATH%;%ProgramFiles%\7-Zip

python build/cmake/python/Start-CMake-SoaringProject.py auto %XCSOAR_TOOLCHAIN% %COMPILE_PARTS%
if errorlevel 1 goto :build_error

echo =====================  Finish ====================================
exit /b 0

:build_error
echo "!!! ERROR !!! ERROR !!! ERROR !!! ERROR !!! ERROR"
echo =====================  Finish ====================================
exit /b 1

:usage
echo Compile-OpenSoar.cmd [toolchain] [steps]
echo    toolchain: msvc2026 (default), msvc2026-OV, msvc2022, mgw122, clang15
echo    steps:     1 configure, 2 build, 4 install, 8 run - add them up (15 = all)
exit /b 0
