// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "PCMSynthesiser.hpp"

#include <atomic>

/**
 * This class generates tones with a sine wave.
 */
class ToneSynthesiser : public PCMSynthesiser {
  /** the amplitude in 1/1024 of the full level, see SetVolume() */
  std::atomic<unsigned> gain{1024};
  unsigned angle = 0, increment = 0;

public:
  explicit ToneSynthesiser(unsigned _sample_rate) : sample_rate(_sample_rate) {
  }

  unsigned GetSampleRate() const {
    return sample_rate;
  }

  /**
   * The amplitude in 1/1024 of the full level for a volume in percent.
   * The ear hears the level in decibels, so a linear amplitude made
   * 50 % sound almost as loud as 100 % (only 6 dB less) and crowded
   * all the audible change into the lowest settings.  Every 10 % are
   * 3 dB instead: 100 % is the full level, 50 % is 15 dB below it and
   * clearly softer, 10 % is 27 dB below it, and 0 is silent.
   */
  [[gnu::const]]
  static unsigned VolumeToGain(unsigned percent) noexcept;

  /**
   * Set the (software) volume of the generated tone.
   *
   * @param _volume the new volume level, 0 indicating muted, 100
   * means full volume; see VolumeToGain()
   */
  void SetVolume(unsigned _volume) noexcept {
    gain = VolumeToGain(_volume);
  }

  void SetTone(unsigned tone_hz);

  /* methods from class PCMSynthesiser */
  virtual void Synthesise(int16_t *buffer, size_t n);

protected:
  const unsigned sample_rate;

  /**
   * Returns the number of samples until the sample value gets close
   * to zero.
   */
  [[gnu::pure]]
  unsigned ToZero() const;

  /**
   * Start a new period.
   */
  void Restart() {
    angle = 0;
  }
};
