// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

struct VarioSoundSettings;

/**
 * Apply the vario sound settings.  The program's own synthesiser plays
 * only if it is the selected source; on the OpenVario, variod gets the
 * volume if it is the selected one and 0 otherwise, so that the two
 * vario tones never play at once on the same audio output.
 */
void
ApplyVarioSound(const VarioSoundSettings &settings) noexcept;

/**
 * Called by ProcessTimer(): hands a volume for variod to the driver of
 * the OpenVario device as soon as that device is open, because the
 * settings are applied before the devices are.
 */
void
VarioSoundTick() noexcept;
