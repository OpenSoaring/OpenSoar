// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#ifdef STOP_WATCH

#include "util/StaticArray.hxx"
#include "LogFile.hpp"

#ifdef HAVE_POSIX
#include <time.h>
#include <cstdint>
#else /* !HAVE_POSIX */
#include <processthreadsapi.h>
#include <profileapi.h>
#endif /* !HAVE_POSIX */

#else /* !STOP_WATCH */

#include "util/StaticArray.hxx"
#include "util/StaticString.hxx"
#include "LogFile.hpp"

#include <chrono>

#endif /* STOP_WATCH */

#ifdef ENABLE_OPENGL
#include "ui/opengl/System.hpp"
#endif

/**
 * A stop watch which measures the time needed to perform an
 * operation, and writes it to the log file.  It is a no-op if the
 * macro STOP_WATCH is not defined.
 */
class ScreenStopWatch {
#ifdef STOP_WATCH
  typedef uint64_t clock_stamp_t;
  typedef uint64_t cpu_stamp_t;

  struct Marker {
    const char *text;
    clock_stamp_t clock;
    cpu_stamp_t cpu;

    void Set(const char *_text) {
      text = _text;
      clock = GetCurrentClock();
      cpu = GetCurrentCPU();
    }
  };

  typedef StaticArray<Marker, 256u> MarkerList;
  MarkerList markers;

private:
  static void FlushScreen() {
#ifdef ENABLE_OPENGL
    glFinish();
#endif
  }

  static clock_stamp_t GetCurrentClock() {
#ifdef HAVE_POSIX
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
#else /* !HAVE_POSIX */
    LARGE_INTEGER l_value, l_frequency;

    if (!::QueryPerformanceCounter(&l_value) ||
        !::QueryPerformanceFrequency(&l_frequency))
      return 0;

    uint64_t value = l_value.QuadPart;
    uint64_t frequency = l_frequency.QuadPart;

    if (frequency > 1000000) {
      value *= 10000;
      value /= frequency / 100;
    } else if (frequency < 1000000) {
      value *= 10000;
      value /= frequency;
      value *= 100;
    }

    return value;
#endif /* !HAVE_POSIX */
  }

  static cpu_stamp_t GetCurrentCPU() {
#ifdef HAVE_POSIX
    // XXX
    return 0;
#else /* !HAVE_POSIX */
    FILETIME f_kernel_time, f_user_time;

    if (!::GetThreadTimes(::GetCurrentThread(), nullptr, nullptr,
                          &f_kernel_time, &f_user_time))
      return 0;

    uint64_t kernel_time = f_kernel_time.dwLowDateTime / 10
      + (uint64_t)f_kernel_time.dwHighDateTime * 100000;
    uint64_t user_time = f_user_time.dwLowDateTime / 10
      + (uint64_t)f_user_time.dwHighDateTime * 100000;

    return kernel_time + user_time;
#endif /* !HAVE_POSIX */
  }

public:
  void Begin() {}

  void Mark(const char *text) {
    FlushScreen();
    markers.append().Set(text);
  }

  void Finish() {
    if (markers.empty())
      return;

    FlushScreen();
    markers.append().Set(nullptr);

    for (unsigned i = 0; markers[i + 1].text != nullptr; ++i) {
      const Marker &start = markers[i];
      const Marker &end = markers[i + 1];

      LogFormat("StopWatch %s: clock=%lu cpu=%lu", start.text,
                (unsigned long)(end.clock - start.clock),
                (unsigned long)(end.cpu - start.cpu));
    }

    const Marker &start = markers.front();
    const Marker &end = markers.back();
    LogFormat("StopWatch total: clock=%lu cpu=%lu",
              (unsigned long)(end.clock - start.clock),
              (unsigned long)(end.cpu - start.cpu));

    markers.clear();
  }

#else /* !STOP_WATCH */
  /* Without STOP_WATCH only frames that take conspicuously long are
     reported, so a map that cannot be drawn in time on a slow device
     shows up in the log together with the layer that costs the time.
     Neither the screen is flushed nor anything logged for a normal
     frame; the cost is a clock reading per marker.  GPU work that the
     driver defers lands in whichever later section waits for it. */
  using Clock = std::chrono::steady_clock;

  struct Marker {
    const char *text;
    Clock::time_point time;
  };

  StaticArray<Marker, 32u> markers;

  /** frames slower than #SLOW_FRAME since the last report */
  unsigned slow_frames = 0;

  /** the slowest of them, already formatted for the log */
  Clock::duration slowest{};
  StaticString<384> slowest_text;

  Clock::time_point last_report{};

  static constexpr std::chrono::milliseconds SLOW_FRAME{300};
  static constexpr std::chrono::seconds REPORT_INTERVAL{10};

  static unsigned ToMilliseconds(Clock::duration d) noexcept {
    return std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
  }

public:
  /**
   * Start a new frame.  Markers left over from an earlier frame are
   * dropped, so the idle time between two frames is never counted.
   */
  void Begin() noexcept {
    markers.clear();
    Mark("Prepare");
  }

  void Mark(const char *text) noexcept {
    if (!markers.full())
      markers.append({text, Clock::now()});
  }

  void Finish() noexcept {
    if (markers.empty())
      return;

    const auto end = Clock::now();
    const auto total = end - markers.front().time;
    if (total < SLOW_FRAME) {
      markers.clear();
      return;
    }

    ++slow_frames;

    if (total > slowest) {
      /* name the sections that took at least a tenth of the frame */
      slowest = total;
      slowest_text.Format("%u ms:", ToMilliseconds(total));
      for (unsigned i = 0; i < markers.size(); ++i) {
        const auto section_end = i + 1 < markers.size()
          ? markers[i + 1].time
          : end;
        const auto section = section_end - markers[i].time;
        if (section * 10 >= total)
          slowest_text.AppendFormat(" %s %u ms", markers[i].text,
                                    ToMilliseconds(section));
      }
    }

    markers.clear();

    if (last_report != Clock::time_point{} &&
        end - last_report < REPORT_INTERVAL)
      return;

    LogFormat("Slow map frames: %u since the last report, the slowest %s",
              slow_frames, slowest_text.c_str());

    slow_frames = 0;
    slowest = {};
    last_report = end;
  }
#endif /* !STOP_WATCH */
};
