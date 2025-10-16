@echo off
cls
cd /D %~dp0../..

::=======================
set OPENSOAR_TOOLCHAIN=%~1
rem if "%OPENSOAR_TOOLCHAIN%" == "" set OPENSOAR_TOOLCHAIN=msvc2022
if not defined OPENSOAR_TOOLCHAIN set OPENSOAR_TOOLCHAIN=msvc2022
set COMPILE_PARTS=%~2
if not defined COMPILE_PARTS  set COMPILE_PARTS=15


echo %CD%
echo OPENSOAR_TOOLCHAIN = %OPENSOAR_TOOLCHAIN%
PATH=%CD%;%CD%\build\cmake\python;%PATH%

REM pause

python build/cmake/python/Start-CMake-OpenSoar.py  opensoar %OPENSOAR_TOOLCHAIN% %COMPILE_PARTS%

:: if errorlevel 1 pause
if errorlevel 1 echo "!!! ERROR !!! ERROR !!! ERROR !!! ERROR !!! ERROR"
echo "??? ERROR !!! ERROR !!! ERROR !!! ERROR !!! ERROR"

