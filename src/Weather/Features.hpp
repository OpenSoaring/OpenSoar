// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "net/http/Features.hpp"

#ifdef HAVE_HTTP
# define HAVE_NOAA
// # define HAVE_RASP
# define HAVE_PCMET
// defined in features.mk (or CMake): # define HAVE_SKYSIGHT
#endif
