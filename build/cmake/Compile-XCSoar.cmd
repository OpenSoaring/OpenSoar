@echo off
setlocal
cd /D %~dp0../..

::  Compile-XCSoar.cmd  [toolchain]  [steps]
::
::  Builds the brand-neutral UPSTREAM COMPARISON project: current XCSoar
::  master plus only the MSVC/CMake enablement (topic/msvc-compat), in a
::  separate git worktree beside this repository.  The OpenSoar solution
::  from Compile-OpenSoar.cmd is not touched - the two live in different
::  build directories.
::
::  Arguments are the same as for Compile-OpenSoar.cmd; -h explains them.

if /I "%~1" == "-h"     goto :usage
if /I "%~1" == "--help" goto :usage
if /I "%~1" == "/?"     goto :usage

set "WT=%CD%\..\XCSoar-upstream"

if not exist "%WT%\.git" (
  echo === creating upstream worktree: %WT%
  git worktree add --detach "%WT%" topic/msvc-compat || exit /b 1
) else (
  echo === updating upstream worktree to topic/msvc-compat
  git -C "%WT%" checkout --detach -q topic/msvc-compat || exit /b 1
)

call "%WT%\build\cmake\Compile-OpenSoar.cmd" %*
exit /b %errorlevel%

:usage
echo Compile-XCSoar.cmd [toolchain] [steps]
echo    builds upstream XCSoar (topic/msvc-compat) in the worktree
echo    ..\XCSoar-upstream - arguments as in Compile-OpenSoar.cmd
exit /b 0
