// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "GlueRecorderDownload.hpp"
#include "ExternalLogger.hpp"
#include "Blackboard/LiveBlackboard.hpp"
#include "NMEA/MoreData.hpp"
#include "NMEA/Derived.hpp"
#include "Computer/Settings.hpp"
#include "Language/Language.hpp"
#include "Message.hpp"
#include "LogFile.hpp"
#include "util/StaticString.hxx"

/**
 * How long after the confirmed landing the download starts.  A logger
 * closes its file only when it has decided that the flight is over
 * (FLARM after about a minute at rest); before that the flight would
 * be missing or incomplete.
 */
static constexpr auto DOWNLOAD_DELAY = std::chrono::minutes{2};

GlueRecorderDownload::GlueRecorderDownload(LiveBlackboard &_blackboard) noexcept
  :blackboard(_blackboard)
{
  blackboard.AddListener(*this);
}

GlueRecorderDownload::~GlueRecorderDownload() noexcept
{
  blackboard.RemoveListener(*this);
}

void
GlueRecorderDownload::OnCalculatedUpdate(const MoreData &basic,
                                         const DerivedInfo &calculated)
{
  /* like the log book: a replay or the simulator is not a flight of
     the pilot */
  if (basic.gps.replay || basic.gps.simulator)
    return;

  const bool flying = calculated.flight.flying;
  if (flying == last_flying)
    return;

  last_flying = flying;

  if (flying)
    /* in the air again before the download (a touch and go, a winch
       launch after a short hop) */
    timer.Cancel();
  else
    timer.Schedule(DOWNLOAD_DELAY);
}

void
GlueRecorderDownload::OnTimer() noexcept
{
  if (blackboard.Calculated().flight.flying)
    return;

  const auto &logger = blackboard.GetComputerSettings().logger;
  for (unsigned i = 0; i < 2; ++i) {
    if (logger.recorder_devices[i] == 0)
      continue;

    /* the log says why a flight is missing in the log book */
    LogFmt("Recorder {}: automatic download after the landing", i + 1);
    if (ExternalLogger::DownloadFromRecorder(i, true)) {
      LogFmt("Recorder {}: flight downloaded", i + 1);
      StaticString<64> text;
      text.Format(_("Recorder %u: flight downloaded"), i + 1);
      Message::AddMessage(text);
    } else
      LogFmt("Recorder {}: nothing downloaded (not connected, no new "
             "flight or an error)", i + 1);
  }
}
