// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

/**
 * Device slot layout:
 *
 *   0 .. 6              regular, user-configurable ports
 *   7  INTERNAL_DEVICE_SLOT  internal sensors (Android / Apple)
 *   8  REMOTE_PORT      SteFly RemoteStick, auto-detected at startup
 *                       (never persisted to the profile; the row is
 *                       only shown in the device list while a stick
 *                       is attached)
 *
 * REMOTE_PORT is appended after the upstream slots so all upstream
 * indices (and thus profile keys) stay unchanged.
 */
static constexpr unsigned INTERNAL_DEVICE_SLOT = 7;
#ifdef HAVE_REMOTE_STICK
static constexpr unsigned REMOTE_PORT = 8;
static constexpr unsigned NUMDEV = 9;
#else
static constexpr unsigned NUMDEV = 8;
#endif
/** number of rows shown in the device list when no RemoteStick is
    attached (= all slots except REMOTE_PORT) */
static constexpr unsigned VISIBLE_NUMDEV = 8;

#if defined(ANDROID) || defined(__APPLE__)
#define HAVE_INTERNAL_GPS
#endif
