// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "VarioSettings.hpp"

void
VarioSoundSettings::SetDefaults()
{
#ifdef IS_OPENVARIO
  /* the OpenVario is a vario: it has always played one, until now by
     variod, so the program's own sound is on from the start */
  enabled = true;
#else
  enabled = false;
#endif
  volume = 80;
  switching_mode = VarioSoundSwitchingMode::MANUAL;
  manual_mode = VarioSoundManualMode::VARIO;
  /* the program's own sound follows the settings of the program
     (dead band, speed to fly, frequencies) and the master volume;
     variod is the fallback a pilot chooses on purpose */
  source = VarioSoundSource::INTERNAL;
  dead_band_enabled = false;

  min_frequency = 200;
  zero_frequency = 500;
  max_frequency = 1500;

  min_period_ms = 150;
  max_period_ms = 600;

  min_dead = -0.3;
  max_dead = 0.1;
}
