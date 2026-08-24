@echo off
:: ---------------------------------------------------------------------------
:: Compile-XCSoar.cmd - build the brand-neutral UPSTREAM COMPARISON project:
:: the topic/msvc-compat state (current XCSoar master + only the MSVC/CMake
:: enablement), built in a separate git worktree next to this repo.
::
:: Result: the XCSoar solution under <project_dir>/Binaries/XCSoar/build/...
:: (the OpenSoar solution from Compile-OpenSoar.cmd stays untouched).
::
:: usage: Compile-XCSoar.cmd [toolchain] [parts]     (like Compile-OpenSoar)
:: ---------------------------------------------------------------------------
setlocal
cd /D %~dp0../..
set "WT=%CD%\..\XCSoar-upstream"

if not exist "%WT%\.git" (
  echo === creating upstream worktree: %WT%
  git worktree add --detach "%WT%" topic/msvc-compat || exit /b 1
) else (
  echo === updating upstream worktree to topic/msvc-compat
  git -C "%WT%" checkout --detach -q topic/msvc-compat || exit /b 1
)

call "%WT%\build\cmake\Compile-OpenSoar.cmd" %*
endlocal
