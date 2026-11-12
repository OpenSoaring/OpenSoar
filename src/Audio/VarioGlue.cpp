// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "VarioGlue.hpp"
#include "PCMPlayer.hpp"
#include "PCMPlayerFactory.hpp"
#include "VarioSynthesiser.hpp"
#include "VarioSettings.hpp"
#include "LogFile.hpp"

#ifdef ANDROID
#include "SLES/Init.hpp"
#endif

#include <atomic>
#include <cassert>

static constexpr unsigned sample_rate = 44100;

#ifdef ANDROID
static bool have_sles;
#endif

static PCMPlayer *player;
static VarioSynthesiser *synthesiser;
static std::atomic<VarioSoundSwitchingMode> switching_mode{
  VarioSoundSwitchingMode::MANUAL
};
static std::atomic<VarioSoundManualMode> manual_mode{
  VarioSoundManualMode::VARIO
};

bool
AudioVarioGlue::HaveAudioVario()
{
#ifdef ANDROID
  return have_sles;
#else
  return true;
#endif
}

void
AudioVarioGlue::Initialise()
{
  assert(player == nullptr);
  assert(synthesiser == nullptr);

#ifdef ANDROID
  have_sles = SLES::Initialise();
  if (!have_sles)
    return;
#endif

  player = PCMPlayerFactory::CreateInstance();
  synthesiser = new VarioSynthesiser(sample_rate);
}

void
AudioVarioGlue::Deinitialise()
{
  delete player;
  player = nullptr;
  delete synthesiser;
  synthesiser = nullptr;
}

void
AudioVarioGlue::Configure(const VarioSoundSettings &settings)
{
#ifdef ANDROID
  if (!have_sles)
    return;
#endif

  assert(player != nullptr);
  assert(synthesiser != nullptr);

  manual_mode = settings.manual_mode;
  switching_mode = settings.switching_mode;

  /* Configure() runs for every change of a setting; the log tells only
     when the vario starts or stops, and whether the audio device could
     be opened, so that a silent vario can be traced from the log */
  static bool was_enabled = false;
  if (settings.enabled != was_enabled) {
    was_enabled = settings.enabled;
    LogFmt("Audio vario {}, volume {}%",
           settings.enabled ? "on" : "off", settings.volume);
  }

  if (settings.enabled) {
    synthesiser->SetVolume(settings.volume);
    synthesiser->SetDeadBand(settings.dead_band_enabled);
    synthesiser->SetFrequencies(settings.min_frequency, settings.zero_frequency,
                                settings.max_frequency);
    synthesiser->SetPeriods(settings.min_period_ms, settings.max_period_ms);
    synthesiser->SetDeadBandRange(settings.min_dead, settings.max_dead);
    if (!player->Start(*synthesiser))
      LogFmt("Audio vario: the sound output could not be started");
  } else
    player->Stop();
}

void
AudioVarioGlue::SetValue(const VarioAudioInput &input)
{
#ifdef ANDROID
  if (!have_sles)
    return;
#endif

  assert(player != nullptr);
  assert(synthesiser != nullptr);

  const auto value = ComputeAudioValue(input, switching_mode.load(),
                                       manual_mode.load());
  if (value)
    synthesiser->SetVario(*value);
  else
    synthesiser->SetSilence();
}
