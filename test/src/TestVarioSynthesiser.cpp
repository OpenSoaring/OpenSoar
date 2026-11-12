// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Audio/VarioSynthesiser.hpp"
#include "Audio/VarioSettings.hpp"
#include "TestUtil.hpp"

#include <algorithm>
#include <array>

static bool
IsSilent(double vario, bool dead_band_enabled,
         double min_dead=-0.3, double max_dead=0.1)
{
  VarioSynthesiser synthesiser(44100);
  synthesiser.SetDeadBand(dead_band_enabled);
  synthesiser.SetDeadBandRange(min_dead, max_dead);
  synthesiser.SetVario(vario);

  std::array<int16_t, 512> buffer{};
  synthesiser.Synthesise(buffer.data(), buffer.size());
  return std::all_of(buffer.begin(), buffer.end(),
                     [](int16_t sample) { return sample == 0; });
}

int
main()
{
  plan_tests(15);

  ok1(!IsSilent(0, false));
  ok1(IsSilent(0, true));
  ok1(IsSilent(-0.3, true));
  ok1(IsSilent(0.1, true));
  ok1(!IsSilent(0.2, true));

  /* the volume in steps of 3 dB per 10 percent */
  ok1(ToneSynthesiser::VolumeToGain(0) == 0);
  ok1(ToneSynthesiser::VolumeToGain(100) == 1024);
  ok1(ToneSynthesiser::VolumeToGain(90) == 725);  /* -3 dB */
  ok1(ToneSynthesiser::VolumeToGain(50) == 182);  /* -15 dB */
  ok1(ToneSynthesiser::VolumeToGain(10) == 46);   /* -27 dB */
  ok1(ToneSynthesiser::VolumeToGain(1) > 0);

  /* a stored volume goes to the nearest step of 10 percent */
  ok1(VarioSoundSettings::RoundVolume(48) == 50);
  ok1(VarioSoundSettings::RoundVolume(44) == 40);
  ok1(VarioSoundSettings::RoundVolume(5) == 10);
  ok1(VarioSoundSettings::RoundVolume(200) == 100);

  return exit_status();
}
