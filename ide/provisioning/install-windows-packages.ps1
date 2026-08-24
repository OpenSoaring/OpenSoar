<#
.SYNOPSIS
  Install the build dependencies for the CMake/Visual Studio (and MinGW /
  clang) builds of XCSoar on a fresh Windows machine.

  Windows counterpart of install-debian-packages.sh: everything comes from
  winget (built into Windows 10 21H2+ / Windows 11), so the machine only
  needs an internet connection.  Re-running is safe - already installed
  packages are skipped.

.DESCRIPTION
  Sections (default: BASE MSVC):
    BASE   git, python3, Strawberry Perl (also brings xsltproc), CMake,
           Ninja, 7-Zip, ImageMagick, Inkscape        - always required
    MSVC   Visual Studio 2022 Build Tools with the C++ workload (cl, link,
           MSBuild, Windows SDK) - toolchain msvc2022 / msvc2026 (OpenGL)
    MINGW  MSYS2 (mingw-w64 gcc/g++, make)             - toolchain mgwXXX
    CLANG  LLVM (clang, clang-cl, lld, llvm-rc)          - toolchain clangXX
    ENV    set the XCSOAR_* user environment variables (project root,
           link_libs, third-party) - see build/cmake/python/CMakeXCSoar.py

  -Check    only report what is installed / missing, install nothing
  -Root     project root for the ENV section (default C:\Projects)

.EXAMPLE
  # from an *administrator* PowerShell (winget needs it for machine-wide
  # installs like the VS Build Tools):
  Set-ExecutionPolicy -Scope Process Bypass
  .\ide\provisioning\install-windows-packages.ps1
  .\ide\provisioning\install-windows-packages.ps1 BASE MSVC MINGW ENV
  .\ide\provisioning\install-windows-packages.ps1 -Check
#>
[CmdletBinding()]
param(
  [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
  [string[]] $Sections = @('BASE', 'MSVC'),
  [switch] $Check,
  [string] $Root = 'C:\Projects'
)

$ErrorActionPreference = 'Stop'

# --------------------------------------------------------------------------
# package table: winget id -> command that proves it is installed
# --------------------------------------------------------------------------
$Packages = @{
  BASE = @(
    @{ Id = 'Git.Git';                    Cmd = 'git';       Name = 'Git' },
    @{ Id = 'Python.Python.3.13';         Cmd = 'python';    Name = 'Python 3' },
    @{ Id = 'StrawberryPerl.StrawberryPerl'; Cmd = 'perl';   Name = 'Strawberry Perl (perl, xsltproc)' },
    @{ Id = 'Kitware.CMake';              Cmd = 'cmake';     Name = 'CMake' },
    @{ Id = 'Ninja-build.Ninja';          Cmd = 'ninja';     Name = 'Ninja' },
    @{ Id = '7zip.7zip';                  Cmd = '7z';        Name = '7-Zip';
       Path = 'C:\Program Files\7-Zip' },
    @{ Id = 'ImageMagick.ImageMagick';    Cmd = 'magick';    Name = 'ImageMagick 7' },
    @{ Id = 'Inkscape.Inkscape';          Cmd = 'inkscape';  Name = 'Inkscape';
       Path = 'C:\Program Files\Inkscape\bin' }
  )
  MSVC = @(
    @{ Id = 'Microsoft.VisualStudio.2022.BuildTools'; Cmd = $null; Name = 'VS 2022 Build Tools (C++ workload)';
       Override = '--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --add Microsoft.VisualStudio.Component.VC.CMake.Project';
       Probe = { Test-VsCpp } }
  )
  MINGW = @(
    @{ Id = 'MSYS2.MSYS2';                Cmd = $null;       Name = 'MSYS2 (mingw-w64)';
       Probe = { Test-Path 'C:\msys64\usr\bin\bash.exe' };
       Post  = { Install-MinGWToolchain } }
  )
  CLANG = @(
    @{ Id = 'LLVM.LLVM';                  Cmd = 'clang';     Name = 'LLVM/clang';
       Path = 'C:\Program Files\LLVM\bin' }
  )
}

# --------------------------------------------------------------------------
# helpers
# --------------------------------------------------------------------------
function Write-Step($msg) { Write-Host "=== $msg" -ForegroundColor Cyan }
function Write-Ok($msg)   { Write-Host "  [ok]      $msg" -ForegroundColor Green }
function Write-Miss($msg) { Write-Host "  [missing] $msg" -ForegroundColor Yellow }

function Test-Cmd($cmd, $extraPath) {
  if (-not $cmd) { return $false }
  if (Get-Command $cmd -ErrorAction SilentlyContinue) { return $true }
  if ($extraPath -and (Test-Path (Join-Path $extraPath "$cmd.exe"))) { return $true }
  return $false
}

function Test-VsCpp {
  $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
  if (-not (Test-Path $vswhere)) { return $false }
  $p = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
  return [bool] $p
}

function Test-Winget {
  if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    throw "winget not found - install 'App Installer' from the Microsoft Store (Windows 10 21H2+ / 11), then re-run"
  }
}

function Install-Package($pkg) {
  $args = @('install', '--id', $pkg.Id, '--exact', '--silent',
            '--accept-package-agreements', '--accept-source-agreements')
  if ($pkg.Override) { $args += @('--override', $pkg.Override) }
  Write-Host "  winget $($args -join ' ')" -ForegroundColor DarkGray
  & winget @args
  if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne -1978335189) {   # -1978335189 = already installed
    Write-Warning "winget returned $LASTEXITCODE for $($pkg.Id)"
  }
}

function Install-MinGWToolchain {
  # inside MSYS2: the UCRT64 gcc toolchain + make (what the mgw toolchains expect)
  $bash = 'C:\msys64\usr\bin\bash.exe'
  if (-not (Test-Path $bash)) { return }
  Write-Host "  installing mingw-w64-ucrt-x86_64 toolchain in MSYS2 ..." -ForegroundColor DarkGray
  & $bash -lc 'pacman -Syu --noconfirm --needed mingw-w64-ucrt-x86_64-toolchain make' | Out-Null
}

function Set-UserEnv($name, $value) {
  $cur = [Environment]::GetEnvironmentVariable($name, 'User')
  if ($cur -eq $value) { Write-Ok "$name = $value"; return }
  if ($Check) { Write-Miss "$name (would be set to $value)"; return }
  [Environment]::SetEnvironmentVariable($name, $value, 'User')
  Write-Ok "$name = $value  (set)"
}

# --------------------------------------------------------------------------
# main
# --------------------------------------------------------------------------
$Sections = $Sections | ForEach-Object { $_.ToUpperInvariant() }
if (-not $Check) { Test-Winget }

foreach ($section in $Sections) {
  if ($section -eq 'ENV') {
    Write-Step 'ENV: XCSOAR_* user environment variables'
    Set-UserEnv 'XCSOAR_PROJECT_DIR'  $Root
    Set-UserEnv 'XCSOAR_LINK_LIBS'    (Join-Path $Root 'link_libs')
    Set-UserEnv 'XCSOAR_THIRD_PARTY'  (Join-Path $Root 'Libs')
    Set-UserEnv 'MAGICK_THREAD_LIMIT' '1'
    Set-UserEnv 'PYTHONUTF8'          '1'
    foreach ($d in @($Root, (Join-Path $Root 'link_libs'), (Join-Path $Root 'Libs'),
                     (Join-Path $Root 'Binaries'), (Join-Path $Root 'OpenSoaring'))) {
      if (-not (Test-Path $d)) { if (-not $Check) { New-Item -ItemType Directory $d | Out-Null }; Write-Ok "mkdir $d" }
    }
    continue
  }
  if (-not $Packages.ContainsKey($section)) {
    Write-Warning "unknown section '$section' (known: $($Packages.Keys -join ' ') ENV)"; continue
  }
  Write-Step "$section"
  foreach ($pkg in $Packages[$section]) {
    $present = if ($pkg.Probe) { & $pkg.Probe } else { Test-Cmd $pkg.Cmd $pkg.Path }
    if ($present) { Write-Ok $pkg.Name; continue }
    if ($Check)   { Write-Miss "$($pkg.Name)  (winget id $($pkg.Id))"; continue }
    Write-Host "  installing $($pkg.Name) ..."
    Install-Package $pkg
    if ($pkg.Post) { & $pkg.Post }
  }
}

if (-not $Check) {
  Write-Host ''
  Write-Step 'done - open a NEW terminal so PATH/env changes are picked up, then:'
  Write-Host '  cd <repo> && build\cmake\Compile-OpenSoar.cmd msvc2026 1    (configure only)'
  Write-Host '  cd <repo> && build\cmake\Compile-OpenSoar.cmd msvc2026      (build the fork)'
Write-Host '  cd <repo> && build\cmake\Compile-XCSoar.cmd msvc2026        (upstream comparison build)'
  Write-Host '  the first configure builds the third-party libraries into link_libs (takes a while)'
}
