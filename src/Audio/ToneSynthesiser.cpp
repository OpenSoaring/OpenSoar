// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ToneSynthesiser.hpp"
#include "Math/FastTrig.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

unsigned
ToneSynthesiser::VolumeToGain(unsigned percent) noexcept
{
  if (percent == 0)
    return 0;
  if (percent >= 100)
    return 1024;

  /* 3 dB per 10 percent: 20 * log10(gain) = -0.3 * (100 - percent) */
  const double db = -0.3 * (100 - percent);
  return std::max(1u, (unsigned)std::lround(1024 * std::pow(10., db / 20)));
}

void
ToneSynthesiser::SetTone(unsigned tone_hz)
{
  increment = ISINETABLE.size() * tone_hz / sample_rate;
}

void
ToneSynthesiser::Synthesise(int16_t *buffer, size_t n)
{
  assert(angle < ISINETABLE.size());
  const int current_gain = gain.load();

  for (int16_t *end = buffer + n; buffer != end; ++buffer) {
    *buffer = ISINETABLE[angle] * (32767 / 1024) * current_gain / 1024;
    angle = (angle + increment) & (ISINETABLE.size() - 1);
  }
}

unsigned
ToneSynthesiser::ToZero() const
{
  assert(angle < ISINETABLE.size());

  if (angle < increment)
    /* close enough */
    return 0;

  return (ISINETABLE.size() - angle) / increment;
}
