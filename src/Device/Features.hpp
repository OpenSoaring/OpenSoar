// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once


static constexpr unsigned VISIBLE_NUMDEV = 6;
static constexpr unsigned REMOTE_PORT = VISIBLE_NUMDEV;  // the last one
static constexpr unsigned NUMDEV = VISIBLE_NUMDEV+1;

#if defined(ANDROID) || defined(__APPLE__)
#define HAVE_INTERNAL_GPS
#endif
