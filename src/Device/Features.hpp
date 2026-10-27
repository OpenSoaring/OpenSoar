// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

/**
 * Device slot layout:
 *
 *   0 .. 6              regular, user-configurable ports
 *   7  INTERNAL_DEVICE_SLOT  internal sensors (Android / Apple)
 *   8  REMOTE_PORT      SteFly RemoteStick, auto-detected at startup
 *   9  ROTARY_PORT      SteFly RotaryPanel, auto-detected at startup
 *
 * The two SteFly slots are filled by the program itself from the USB
 * ids it finds at startup, with a fixed driver; the pilot cannot edit
 * them, and their rows are only shown in the device list while the
 * device is attached.  They are appended after the upstream slots so
 * all upstream indices (and thus profile keys) stay unchanged.
 */
static constexpr unsigned INTERNAL_DEVICE_SLOT = 7;
#ifdef HAVE_REMOTE_STICK
static constexpr unsigned REMOTE_PORT = 8;
static constexpr unsigned ROTARY_PORT = 9;
static constexpr unsigned NUMDEV = 10;
#else
static constexpr unsigned NUMDEV = 8;
#endif
/** number of slots shown in the device list in any case (= all slots
    except the auto-detected SteFly ones) */
static constexpr unsigned VISIBLE_NUMDEV = 8;

/**
 * Is this one of the slots the program fills by itself with an
 * auto-detected SteFly device?
 */
constexpr bool
IsSteFlySlot([[maybe_unused]] unsigned i) noexcept
{
#ifdef HAVE_REMOTE_STICK
  return i == REMOTE_PORT || i == ROTARY_PORT;
#else
  return false;
#endif
}

#if defined(ANDROID) || defined(__APPLE__)
#define HAVE_INTERNAL_GPS
#endif
