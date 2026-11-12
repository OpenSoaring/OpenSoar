// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

/**
 * The quick dialog for the sound in flight: master and vario volume as
 * bars, a mute button for the vario and, on the OpenVario, the choice
 * between the program's own vario sound and variod.  Every change is
 * heard at once.
 */
void
ShowAudioDialog() noexcept;
