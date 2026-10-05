// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Blackboard/BlackboardListener.hpp"
#include "ui/event/Timer.hpp"

class LiveBlackboard;

/**
 * Downloads the flight just landed from Recorder 1 and 2 (logger
 * settings) on its own: a while after the landing, when the recorders
 * have closed their files, the newest flight of each is downloaded
 * into the folder of the IGC files and added to the log book.
 *
 * Replays and the simulator are no flights of the pilot and start no
 * download.
 */
class GlueRecorderDownload final : private NullBlackboardListener {
  LiveBlackboard &blackboard;

  UI::Timer timer{[this]{ OnTimer(); }};

  bool last_flying = false;

public:
  explicit GlueRecorderDownload(LiveBlackboard &blackboard) noexcept;
  ~GlueRecorderDownload() noexcept;

  GlueRecorderDownload(const GlueRecorderDownload &) = delete;
  GlueRecorderDownload &operator=(const GlueRecorderDownload &) = delete;

private:
  void OnTimer() noexcept;

  /* virtual methods from class BlackboardListener */
  void OnCalculatedUpdate(const MoreData &basic,
                          const DerivedInfo &calculated) override;
};
