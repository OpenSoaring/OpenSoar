// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <cstdint>
#include <type_traits>

enum class VarioSoundSwitchingMode : uint8_t {
  MANUAL,
  AUTO,
};

enum class VarioSoundManualMode : uint8_t {
  VARIO,
  STF,
};

/**
 * Which program makes the vario sound.  On an OpenVario both the
 * program and the vario daemon (variod) can play one on the same
 * audio output, and two vario tones at once cannot be told apart;
 * only the selected one gets the volume, the other is set to 0.
 */
enum class VarioSoundSource : uint8_t {
  /** the program's own synthesiser (AudioVarioGlue) */
  INTERNAL,

  /** variod on the OpenVario, through the OpenVario device driver */
  OPENVARIO,
};

/**
 * The one setting "Audio Vario": off, the program's own sound, or
 * variod.  It is stored in the profile as the number of this enum, so
 * that the "1" of a profile from before means the program's sound.
 */
enum class VarioSoundMode : uint8_t {
  OFF,
  PROGRAM,
  VARIOD,
};

struct VarioSoundSettings {
  bool enabled;
  uint8_t volume;
  VarioSoundSwitchingMode switching_mode;
  VarioSoundManualMode manual_mode;
  VarioSoundSource source;
  bool dead_band_enabled;

  unsigned min_frequency;
  unsigned zero_frequency;
  unsigned max_frequency;

  unsigned min_period_ms;
  unsigned max_period_ms;

  double min_dead;
  double max_dead;

  void SetDefaults();

  /**
   * The mode from #enabled and #source: with variod as the source,
   * #enabled does not count, because only the program's own sound is
   * switched by it.
   */
  [[gnu::pure]]
  VarioSoundMode GetMode() const noexcept {
    if (source == VarioSoundSource::OPENVARIO)
      return VarioSoundMode::VARIOD;
    return enabled ? VarioSoundMode::PROGRAM : VarioSoundMode::OFF;
  }

  void SetMode(VarioSoundMode mode) noexcept {
    enabled = mode != VarioSoundMode::OFF;
    source = mode == VarioSoundMode::VARIOD
      ? VarioSoundSource::OPENVARIO
      : VarioSoundSource::INTERNAL;
  }

  /**
   * The vario volume goes in steps of 10 percent, 3 dB each (see
   * ToneSynthesiser::VolumeToGain()); this is the nearest step.
   */
  static constexpr uint8_t RoundVolume(unsigned percent) noexcept {
    return percent >= 100 ? 100 : (percent + 5) / 10 * 10;
  }
};

static_assert(std::is_trivial<VarioSoundSettings>::value, "type is not trivial");
