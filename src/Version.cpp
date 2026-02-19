// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Version.hpp"
#include "ProgramVersion.h"

#ifndef PROGRAM_VERSION
#error Macro "PROGRAM_VERSION" is not defined.  Check build/version.mk!
#endif

#define VERSION PROGRAM_VERSION

#if defined(ANDROID)
  #define TARGET "Android"
#elif defined(KOBO)
  #define TARGET "Kobo"
#elif defined(IS_OPENVARIO)
  #define TARGET "OpenVario"
#elif defined(__linux__)
  #define TARGET "Linux"
#elif defined(__APPLE__)
  #include <TargetConditionals.h>
  #if TARGET_OS_IPHONE
    #define TARGET "iOS"
  #else
    #define TARGET "macOS"
  #endif
#elif !defined(_WIN32)
  #define TARGET "UNIX"
#elif !defined(WIN64)
# define TARGET "WIN64"
#else
# define TARGET "WIN32"
#endif

#define VERSION_SUFFIX ""

#ifdef GIT_COMMIT
# define GIT_SUFFIX " git: " GIT_COMMIT
  const char App_GitCommit[] = GIT_COMMIT;
#else
# define GIT_SUFFIX
  const char App_GitCommit[] = "";
#endif

const char App_Name[] = PROGRAM_NAME;
const char App_Version[] = VERSION;
const char App_VersionLong[] = VERSION VERSION_SUFFIX;
const char App_VersionString[] = VERSION VERSION_SUFFIX "-" TARGET;
const char App_VersionStringOld[] = TARGET " " VERSION VERSION_SUFFIX;
const char App_ProductToken[] = PROGRAM_NAME " v" VERSION VERSION_SUFFIX
                                                   "-" TARGET GIT_SUFFIX;
