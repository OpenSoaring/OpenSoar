// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Replay.hpp"
#include "IgcReplay.hpp"
#include "NmeaReplay.hpp"
#include "SensorLogReplay.hpp"
#include "DemoReplayGlue.hpp"
#include "io/FileLineReader.hpp"
#include "Blackboard/DeviceBlackboard.hpp"
#include "CalculationThread.hpp"
#include "MergeThread.hpp"
#include "Logger/Logger.hpp"
#include "Interface.hpp"
#include "Protection.hpp"
#include "Repository/FileType.hpp"
#include "CatmullRomInterpolator.hpp"
#include "time/Cast.hxx"

#include <algorithm> // for std::clamp()
#include <cassert>
#include <stdexcept>

bool
Replay::SeekTakeoff() noexcept
{
  if (replay == nullptr || IsSeeking())
    return false;

  seek_mode = SeekMode::TAKEOFF;
  seek_last_location = next_data.location_available
    ? next_data.location : GeoPoint::Invalid();
  seek_last_time = next_data.time_available
    ? next_data.time : TimeStamp::Undefined();

  timer.Schedule(std::chrono::milliseconds(20));
  return true;
}

bool
Replay::SeekTo(TimeStamp target) noexcept
{
  if (replay == nullptr || IsSeeking() || !target.IsDefined())
    return false;

  if (virtual_time.IsDefined() && target <= virtual_time) {
    /* backwards: restart the input file and read forward again */
    if (path == nullptr || path.empty())
      return false;

    const AllocatedPath restart_path{GetFilename()};
    try {
      Start(restart_path);
    } catch (...) {
      return false;
    }
  }

  seek_mode = SeekMode::TIME;
  seek_target = target;

  timer.Schedule(std::chrono::milliseconds(20));
  return true;
}

bool
Replay::PlayToEnd(MergeThread &merge_thread,
                  CalculationThread &calc_thread,
                  FloatDuration interval) noexcept
{
  if (replay == nullptr || IsSeeking())
    return false;

  seek_mode = SeekMode::END;
  end_interval = interval;
  end_last_processed = TimeStamp::Undefined();
  end_merge = &merge_thread;
  end_calc = &calc_thread;

  /* suspend the computer threads once for the whole run - a
     suspend/resume handshake per chunk can wait a long computation
     out every time */
  end_merge->Suspend();
  SuspendAllThreads();
  end_suspended = true;

  timer.Schedule(std::chrono::milliseconds(20));
  return true;
}

inline void
Replay::ResumeEnd() noexcept
{
  if (!end_suspended)
    return;

  end_suspended = false;
  ResumeAllThreads();
  if (end_merge != nullptr)
    end_merge->Resume();
}

bool
Replay::RunSeekChunk() noexcept
{
  /* stay responsive: after this budget the chunk yields back to the
     event loop, and the next timer tick continues */
  const auto deadline = std::chrono::steady_clock::now() +
    std::chrono::milliseconds(40);

  switch (seek_mode) {
  case SeekMode::NONE:
    break;

  case SeekMode::TAKEOFF:
    {
      /* faster than any taxi, slower than any winch launch */
      constexpr double takeoff_speed = 15;

      unsigned n = 0;
      while (true) {
        double speed = -1;
        if (next_data.ground_speed_available)
          speed = next_data.ground_speed;
        else if (next_data.location_available && next_data.time_available &&
                 seek_last_location.IsValid() && seek_last_time.IsDefined() &&
                 next_data.time > seek_last_time)
          /* IGC files have no speed: derive it from the fix distance */
          speed = next_data.location.DistanceS(seek_last_location) /
            (next_data.time - seek_last_time).count();

        if (speed >= takeoff_speed) {
          seek_mode = SeekMode::NONE;
          FinishSeek();
          break;
        }

        if (next_data.location_available)
          seek_last_location = next_data.location;
        if (next_data.time_available)
          seek_last_time = next_data.time;

        if (!replay->Update(next_data)) {
          Stop();
          return false;
        }

        ++fix_count;

        if ((++n & 0xff) == 0 &&
            std::chrono::steady_clock::now() >= deadline)
          break;
      }
    }
    break;

  case SeekMode::TIME:
    {
      unsigned n = 0;
      while (!next_data.time_available || next_data.time < seek_target) {
        if (!replay->Update(next_data)) {
          /* the tape ends before the target: hold paused at the
             last fix instead of ending the replay */
          seek_mode = SeekMode::NONE;
          time_scale = 0;
          FinishSeek();
          return true;
        }

        ++fix_count;

        if ((++n & 0xff) == 0 &&
            std::chrono::steady_clock::now() >= deadline)
          break;
      }

      if (next_data.time_available && next_data.time >= seek_target) {
        seek_mode = SeekMode::NONE;
        FinishSeek();
      }
    }
    break;

  case SeekMode::END:
    {
      bool eof = false;

      while (true) {
        if (!replay->Update(next_data)) {
          eof = true;
          break;
        }

        ++fix_count;

        if (next_data.time_available) {
          virtual_time = next_data.time;

          if (end_last_processed.IsDefined() &&
              next_data.time >= end_last_processed &&
              next_data.time < end_last_processed + end_interval)
            /* too soon after the last processed fix: parse only */
            continue;

          end_last_processed = next_data.time;
        }

        {
          const std::lock_guard lock{device_blackboard.mutex};
          device_blackboard.SetReplayState() = next_data;
        }

        end_merge->ProcessReplayFix();
        end_calc->ProcessReplayFix();

        if (next_data.time_available)
          next_data.Expire();

        if (std::chrono::steady_clock::now() >= deadline)
          break;
      }

      if (eof) {
        /* arrived: hold paused at the landing, with the whole
           flight in trail and statistics */
        ResumeEnd();
        seek_mode = SeekMode::NONE;
        time_scale = 0;

        TriggerCalculatedUpdate();
        TriggerMapUpdate();
      }
    }
    break;
  }

  return true;
}

inline void
Replay::FinishSeek() noexcept
{
  if (next_data.time_available) {
    virtual_time = next_data.time;
    if (cli != nullptr) {
      cli->Reset();
      cli->Update(next_data.time, next_data.location,
                  next_data.gps_altitude, next_data.pressure_altitude);
    }
    clock.Update();

    /* show the new position at once, even while paused - a jump the
       map does not follow would look like nothing happened */
    const std::lock_guard lock{device_blackboard.mutex};
    device_blackboard.SetReplayState() = next_data;
    device_blackboard.ScheduleMerge();
  }

  if (!timer.IsPending())
    timer.Schedule(std::chrono::milliseconds(100));
}

double
Replay::GetProgress() const noexcept
{
  return replay != nullptr ? replay->GetProgress() : -1;
}

void
Replay::Stop()
{
  if (replay == nullptr)
    return;

  timer.Cancel();

  ResumeEnd();
  seek_mode = SeekMode::NONE;
  end_merge = nullptr;
  end_calc = nullptr;

  delete replay;
  replay = nullptr;

  delete cli;
  cli = nullptr;

  device_blackboard.StopReplay();

  if (logger != nullptr)
    logger->ClearBuffer();
}

void
Replay::Start(Path _path)
{
  assert(_path != nullptr);

  /* make sure the old AbstractReplay instance has cleaned up before
     creating a new one */
  Stop();

  path = _path;

  if (path == nullptr || path.empty()) {
    replay = new DemoReplayGlue(device_blackboard, task_manager);
  } else if (FilenameMatchesFileType(path.GetBase().c_str(),
                                      FileType::IGC)) {
    replay = new IgcReplay(std::make_unique<FileLineReaderA>(path));

    cli = new CatmullRomInterpolator(FloatDuration{0.98});
    cli->Reset();
  } else if (FilenameMatchesFileType(path.GetBase().c_str(),
                                     FileType::SENSORLOG)) {
    replay = new SensorLogReplay(path);
  } else {
    replay = new NmeaReplay(std::make_unique<FileLineReaderA>(path),
                            CommonInterface::GetSystemSettings().devices[0]);
  }

  if (logger != nullptr)
    logger->ClearBuffer();

  virtual_time = TimeStamp::Undefined();
  fast_forward = TimeStamp::Undefined();
  next_data.Reset();
  fix_count = 0;
  seek_mode = SeekMode::NONE;

  timer.Schedule(std::chrono::milliseconds(100));
}

bool
Replay::Update()
{
  if (replay == nullptr)
    return false;

  if (time_scale <= 0) {
    /* replay is paused */

    if (!virtual_time.IsDefined()) {
      /* started (or rewound) while paused: read up to the first fix
         so the cursor and the map show where the tape stands */
      while (!next_data.time_available) {
        if (!replay->Update(next_data)) {
          Stop();
          return false;
        }

        ++fix_count;
        assert(!next_data.gps.real);
      }

      virtual_time = next_data.time;
      if (cli != nullptr) {
        cli->Reset();
        cli->Update(next_data.time, next_data.location,
                    next_data.gps_altitude, next_data.pressure_altitude);
      }

      const std::lock_guard lock{device_blackboard.mutex};
      device_blackboard.SetReplayState() = next_data;
      device_blackboard.ScheduleMerge();
    }

    /* to avoid a big fast-forward with the next
       PeriodClock::ElapsedUpdate() call below after unpausing, update
       the clock each time we're called while paused */
    clock.Update();
    return true;
  }

  const auto old_virtual_time = virtual_time;

  if (virtual_time.IsDefined()) {
    /* update the virtual time */
    assert(clock.IsDefined());

    if (!fast_forward.IsDefined()) {
      virtual_time += clock.ElapsedUpdate() * time_scale;
    } else {
      clock.Update();

      virtual_time += std::chrono::seconds{1};
      if (virtual_time >= fast_forward)
        fast_forward = TimeStamp::Undefined();
    }
  } else {
    /* if we ever received a valid time from the AbstractReplay, then
       virtual_time must be initialised */
    assert(!next_data.time_available);
  }

  if (cli == nullptr || fast_forward.IsDefined()) {
    if (next_data.time_available && virtual_time < next_data.time)
      /* still not time to use next_data */
      return true;

    {
      const std::lock_guard lock{device_blackboard.mutex};
      device_blackboard.SetReplayState() = next_data;
      device_blackboard.ScheduleMerge();
    }

    while (true) {
      if (!replay->Update(next_data)) {
        Stop();
        return false;
      }

      ++fix_count;
      assert(!next_data.gps.real);

      if (next_data.time_available) {
        if (!virtual_time.IsDefined()) {
          virtual_time = next_data.time;
          if (fast_forward.IsDefined())
            fast_forward = virtual_time + fast_forward.ToDuration();
          clock.Update();
          break;
        }

        if (next_data.time >= virtual_time)
          break;

        if (next_data.time < old_virtual_time) {
          /* time warp; that can happen on midnight wraparound during
             NMEA replay */
          virtual_time = next_data.time;
          break;
        }
      }
    }
  } else {
    while (cli->NeedData(virtual_time)) {
      if (!replay->Update(next_data)) {
        Stop();
        return false;
      }

      ++fix_count;
      assert(!next_data.gps.real);

      if (next_data.time_available)
        cli->Update(next_data.time, next_data.location,
                    next_data.gps_altitude,
                    next_data.pressure_altitude);
    }

    if (!virtual_time.IsDefined()) {
      virtual_time = cli->GetMaxTime();
      if (fast_forward.IsDefined())
        fast_forward = virtual_time + fast_forward.ToDuration();
      clock.Update();
    }

    const CatmullRomInterpolator::Record r = cli->Interpolate(virtual_time);
    const GeoVector v = cli->GetVector(virtual_time);

    NMEAInfo data = next_data;
    data.clock = virtual_time;
    data.alive.Update(data.clock);
    data.ProvideTime(virtual_time);
    data.location = r.location;
    data.location_available.Update(data.clock);
    data.ground_speed = v.distance;
    data.ground_speed_available.Update(data.clock);
    data.track = v.bearing;
    data.track_available.Update(data.clock);
    data.gps_altitude = r.gps_altitude;
    data.gps_altitude_available.Update(data.clock);
    data.ProvidePressureAltitude(r.baro_altitude);
    data.ProvideBaroAltitudeTrue(r.baro_altitude);

    {
      const std::lock_guard lock{device_blackboard.mutex};
      device_blackboard.SetReplayState() = data;
      device_blackboard.ScheduleMerge();
    }
  }

  return true;
}

unsigned
Replay::ProcessAllFixes(MergeThread &merge_thread,
                        CalculationThread &calc_thread,
                        FloatDuration interval)
{
  if (replay == nullptr || path == nullptr || path.empty())
    return 0;

  timer.Cancel();
  fast_forward = TimeStamp::Undefined();

  NMEAInfo data;
  data.Reset();
  unsigned count = 0;

  /* the time of the last fix fed through the computers: a high-rate
     input (a 10 Hz sensor log, say) is thinned to the requested
     interval - one fix per second is what the computers see from a
     GPS in a real flight, so the statistics stay true while a whole
     flight processes in a fraction of the time */
  TimeStamp last_processed = TimeStamp::Undefined();

  while (replay->Update(data)) {
    assert(!data.gps.real);

    if (data.time_available) {
      virtual_time = data.time;
      ++fix_count;

      if (last_processed.IsDefined() &&
          data.time >= last_processed &&
          data.time < last_processed + interval)
        /* too soon after the last processed fix: parse only */
        continue;

      last_processed = data.time;
    }

    {
      const std::lock_guard lock{device_blackboard.mutex};
      device_blackboard.SetReplayState() = data;
    }

    merge_thread.ProcessReplayFix();
    calc_thread.ProcessReplayFix();
    ++count;

    if (data.time_available)
      data.Expire();
  }

  if (count > 0)
    next_data = data;

  return count;
}

void
Replay::OnTimer()
{
  if (seek_mode != SeekMode::NONE) {
    if (!RunSeekChunk())
      /* the replay died on the way */
      return;

    timer.Schedule(seek_mode != SeekMode::NONE
                   ? std::chrono::milliseconds(20)
                   : std::chrono::milliseconds(100));
    return;
  }

  if (!Update())
    return;

  std::chrono::steady_clock::duration schedule;
  if (time_scale <= 0)
    schedule = std::chrono::seconds(1);
  else if (fast_forward.IsDefined())
    schedule = std::chrono::milliseconds(100);
  else if (!virtual_time.IsDefined() || !next_data.time_available)
    schedule = std::chrono::milliseconds(500);
  else if (cli != nullptr) {
    /* Interpolated IGC emits one GPS sample per timer tick, with
       virtual time advancing by elapsed × rate.  A fixed 1 s wall
       timer therefore produces 10 s flight steps at 10×, which is too
       sparse for circling wind.  Keep flight-time steps near 1 Hz. */
    const double scale = std::max(time_scale, 0.1);
    constexpr std::chrono::steady_clock::duration lower =
      std::chrono::milliseconds(50);
    constexpr std::chrono::steady_clock::duration upper =
      std::chrono::seconds(1);
    const auto period =
      std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        FloatDuration{1} / scale);
    schedule = std::clamp(period, lower, upper);
  } else {
    constexpr std::chrono::steady_clock::duration lower = std::chrono::milliseconds(100);
    constexpr std::chrono::steady_clock::duration upper = std::chrono::seconds(3);
    const FloatDuration delta_s((next_data.time - virtual_time) / time_scale);
    const auto delta = std::chrono::duration_cast<std::chrono::steady_clock::duration>(delta_s);
    schedule = std::clamp(delta, lower, upper);
  }

  timer.Schedule(schedule);
}
