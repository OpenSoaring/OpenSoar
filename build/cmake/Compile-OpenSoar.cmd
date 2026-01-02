@echo off
cls
cd /D %~dp0../..

::=======================
set OPENSOAR_TOOLCHAIN=%~1
:: MinGW: mgw112, mgw122
:: MSVC : msvc2022, msvc2026
:: Clang: clang15 
rem if "%OPENSOAR_TOOLCHAIN%" == "" set OPENSOAR_TOOLCHAIN=msvc2022
if not defined OPENSOAR_TOOLCHAIN set OPENSOAR_TOOLCHAIN=msvc2026
set COMPILE_PARTS=%~2
if not defined COMPILE_PARTS  set COMPILE_PARTS=15


echo %CD%
echo OPENSOAR_TOOLCHAIN = %OPENSOAR_TOOLCHAIN%
PATH=%CD%;%CD%\build\cmake\python;%PATH%

REM pause

python build/cmake/python/Start-CMake-OpenSoar.py  opensoar %OPENSOAR_TOOLCHAIN% %COMPILE_PARTS%

:: if errorlevel 1 pause
if errorlevel 1 echp "!!! ERROR !!! ERROR !!! ERROR !!! ERROR !!! ERROR"


:: mgw112   - 20.12.2025: 
:: mgw122   - 20.12.2025: Ok, allerdings ohne SkySight

:: clang 12 - 07.02.2023: scheinbar ordentlich
::          - 01.03.2023: scheitert jetzt am   sodium 1.0.18?   
:: clang 14 - im toolchainfile muss llvm-ar angegeben werden (ar.exe gibt es nicht!)
:: clang 15 - 07.02.2023: ich weiß auch nicht, wie das schon mal gehen konnte: ich musste ja bei der 15.0.0-Version den Path zu MinGW aufmachen, und da lag ja ein clang12 drin.... Heute auf 15.0.7 geupdated, die toolchain angepasst (llvm-ar und llvm-rc) - danach compilierte er erst einmal durch, hatte nur beim Linken Probleme
::          - 01.03.2023: mit 3 Änderungen lief es besser (durch?)!
::                        * llvm-ar.exe kopiert in ar.exe (boost wollte immer das "ar", obwohl im Toolchain-File 'llvm-ar.exe' als CMAKE_AR_COMPILER angegeben war)
::                        * in (link_libs)/ares_build.h Zeile 5 #define CARES_TYPEOF_ARES_SSIZE_T __int64 (geändert von ...ssize_t)??
::                        * ABER Boost-Json und Boost-Container-Lib wird falsch angefordert: libboost_***-clangw15-mt-gd-x64-1_81.lib
::                          statt libboost_***-clangw15-mt-d-x64-1_81.lib! Warum? eigentlich soll doch das ganze mit HeadersOnly laufen....


