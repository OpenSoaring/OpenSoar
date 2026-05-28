// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

struct DeviceInfo;

/**
 * Show what is known about a connected Larus.  For now this is only
 * the firmware version from the $PLARF sentence; further Larus
 * settings can be added to this dialog later.
 */
void
ManageLarusDialog(const DeviceInfo &info) noexcept;
