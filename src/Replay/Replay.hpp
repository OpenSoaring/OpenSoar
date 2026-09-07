// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ui/event/Timer.hpp"
#include "NMEA/Info.hpp"
#include "time/PeriodClock.hpp"
#include "time/Stamp.hpp"
#include "system/Path.hpp"

class DeviceBlackboard;
class Logger;
class ProtectedTaskManager;
class AbstractReplay;
class CatmullRomInterpolator;
class MergeThread;
class CalculationThread;
class Error;

class Replay final
{
  DeviceBlackboard &device_blackboard;

  UI::Timer timer{[this]{ OnTimer(); }};

  double time_scale = 1;

  AbstractReplay *replay = nullptr;

  Logger *const logger;
  ProtectedTaskManager &task_manager;

  AllocatedPath path = nullptr;

  /**
   * The time of day according to replay input.  This is negative if
   * unknown.
   */
  TimeStamp virtual_time;

  /**
   * If this value is not negative, then we're in fast-forward mode:
   * replay is going as quickly as possible.  This value denotes the
   * time stamp when we will stop going fast-forward.  If
   * #virtual_time is negative, then this is the duration, and
   * #virtual_time will be added as soon as it is known.
   */
  TimeStamp fast_forward;

  /**
   * Keeps track of the wall-clock time between two Update() calls.
   */
  PeriodClock clock;

  /**
   * The last NMEAInfo returned by the #AbstractReplay instance.  It
   * is held back until #virtual_time has passed #next_data.time.
   */
  NMEAInfo next_data;

  CatmullRomInterpolator *cli = nullptr;

  /** the number of fixes read from the input so far */
  unsigned fix_count = 0;

public:
  Replay(DeviceBlackboard &_device_blackboard,
         Logger *_logger, ProtectedTaskManager &_task_manager)
    :device_blackboard(_device_blackboard),
     logger(_logger), task_manager(_task_manager) {}

  ~Replay() {
    Stop();
  }

  bool IsActive() const {
    return replay != nullptr;
  }

private:
  bool Update();

public:
  void Stop();

  /**
   * Throws std::runtime_errror on error.
   */
  void Start(Path _path);

  Path GetFilename() const {
    return path;
  }

  double GetTimeScale() const {
    return time_scale;
  }

  unsigned GetFixCount() const {
    return fix_count;
  }

  bool IsPaused() const {
    return time_scale <= 0;
  }

  /**
   * Read forward to the takeoff: the first fix moving faster than
   * gliders taxi.  The caller restarts the replay first when the
   * cursor may already be beyond that point.  The new position is
   * pushed to the map at once, also while paused.  Returns false
   * when no such fix exists.
   */
  bool SeekTakeoff() noexcept;

  /**
   * Jump to the given time - also backwards, by restarting the
   * input file internally.  The play/pause state is untouched, and
   * the new position is pushed to the map at once.  Returns false
   * when the file ends before that time.
   */
  bool SeekTo(TimeStamp target) noexcept;

  /** 0..1, or negative when unknown */
  [[gnu::pure]]
  double GetProgress() const noexcept;

  void SetTimeScale(const double _time_scale) {
    time_scale = _time_scale;

    /* re-arm the timer: should its schedule chain ever have been
       lost, the next button press revives the replay instead of
       leaving it silently stuck */
    if (replay != nullptr && !timer.IsPending())
      timer.Schedule(std::chrono::milliseconds(100));
  }

  /**
   * Start fast-forwarding the replay by the specified number of
   * seconds.  This replays the given amount of time from the input
   * time as quickly as possible.  Returns false if unable to fast forward.
   */
  bool FastForward(FloatDuration delta_s) noexcept {
    if (!IsActive())
      return false;

    if (!timer.IsPending())
      timer.Schedule(std::chrono::milliseconds(100));

    if (virtual_time.IsDefined()) {
      fast_forward = virtual_time + delta_s;
      return true;
    } else {
      fast_forward = TimeStamp{delta_s};
      return false;
    }
  }

  TimeStamp GetVirtualTime() const noexcept {
    return virtual_time;
  }

  /**
   * Feed the rest of the current replay file through merge and
   * calculation without virtual-time pacing, thinned to \a interval
   * between processed fixes (one per second matches what a GPS
   * delivers in a real flight).  Returns the number of fixes
   * processed (0 if replay is inactive or demo mode).
   * \a merge_thread and \a calc_thread must be suspended.
   */
  unsigned ProcessAllFixes(MergeThread &merge_thread,
                           CalculationThread &calc_thread,
                           FloatDuration interval = std::chrono::seconds{1});

private:
  /**
   * The common tail of the seek functions: fix up the virtual time
   * and the interpolator, show the new position on the map at once
   * (also while paused), and make sure the timer runs.
   */
  void FinishSeek() noexcept;

  void OnTimer();
};
