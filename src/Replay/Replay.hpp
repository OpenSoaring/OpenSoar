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

  /**
   * A running seek: instead of blocking the UI until the target is
   * found, the timer processes the input in small chunks, so the
   * dialog stays alive and its progress bar really moves.
   */
  enum class SeekMode : uint8_t {
    NONE,
    /** read forward until #seek_target */
    TIME,
    /** read forward until the takeoff (SeekTakeoff()) */
    TAKEOFF,
    /** feed the rest through the computers (PlayToEnd()) */
    END,
  } seek_mode = SeekMode::NONE;

  TimeStamp seek_target;

  /** speed derivation state for the TAKEOFF seek (IGC input) */
  GeoPoint seek_last_location;
  TimeStamp seek_last_time;

  /** parameters of the END seek */
  FloatDuration end_interval{};
  TimeStamp end_last_processed;
  MergeThread *end_merge = nullptr;
  CalculationThread *end_calc = nullptr;

  /** are the computer threads suspended for the END seek? */
  bool end_suspended = false;

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
   * Seek forward to the takeoff: the first fix moving faster than
   * gliders taxi.  The caller restarts the replay first when the
   * cursor may already be beyond that point.  The seek runs in
   * chunks from the timer; when it arrives, the new position is
   * pushed to the map, also while paused.
   */
  bool SeekTakeoff() noexcept;

  /**
   * Seek to the given time - also backwards, by restarting the
   * input file internally.  The seek runs in chunks from the timer
   * and leaves the play/pause state untouched; when it arrives, the
   * new position is pushed to the map.
   */
  bool SeekTo(TimeStamp target) noexcept;

  /**
   * Feed the rest of the file through the computers, thinned to \a
   * interval between processed fixes, chunk by chunk from the
   * timer; afterwards the replay holds paused at the last fix, with
   * the whole flight in the trail and the statistics.  The threads
   * are suspended around each chunk.
   */
  bool PlayToEnd(MergeThread &merge_thread,
                 CalculationThread &calc_thread,
                 FloatDuration interval) noexcept;

  /** is a seek (takeoff, time, play-to-end) in progress? */
  bool IsSeeking() const noexcept {
    return seek_mode != SeekMode::NONE;
  }

  bool IsFastForwarding() const noexcept {
    return fast_forward.IsDefined();
  }

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

  /**
   * Process one bounded slice of the running seek.  Returns false
   * when the replay died (input error before the target).
   */
  bool RunSeekChunk() noexcept;

  /** undo the END seek's thread suspension, if active */
  void ResumeEnd() noexcept;

  void OnTimer();
};
