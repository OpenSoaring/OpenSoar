// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ContestResult.hpp"
#include "time/Stamp.hpp"

#include <cstdint>
#include <type_traits>

/**
 * The distances and points of the current flight that the log book
 * records, calculated independently of the contest chosen in the
 * settings: a pilot wants them in the log book whatever contest the
 * contest page shows.
 */
struct LogbookStatistics {
  /**
   * The shape of the best DMSt result; the DMSt rules give each
   * shape its own bonus.
   */
  enum class DMStShape : uint8_t {
    NONE,
    QUADRILATERAL,
    TRIANGLE,
    OUT_AND_RETURN,
  };

  /**
   * The free distance over up to five turn points (six legs), as
   * scored by the OLC classic rules.  The log book uses its distance;
   * the score includes the handicap.
   */
  ContestResult free;

  /**
   * The best DMSt result: its distance is the scored route, its score
   * the DMSt points including bonus and index.
   */
  ContestResult dmst;

  DMStShape dmst_shape;

  /**
   * When the scoring began: the release from the tow or winch, or the
   * end of the engine run of a self-launch.  Undefined before.
   */
  TimeStamp release;

  /**
   * The number of scored parts of the flight.  An engine run in
   * flight ends a part; only the best part counts, as in the contest
   * rules.
   */
  uint8_t scored_parts;

  /**
   * Has the result been calculated exhaustively after the landing?
   * Only then is it final and the log book writes it.
   */
  bool final;

  constexpr void Reset() noexcept {
    free.Reset();
    dmst.Reset();
    dmst_shape = DMStShape::NONE;
    release = TimeStamp::Undefined();
    scored_parts = 0;
    final = false;
  }
};

static_assert(std::is_trivial<LogbookStatistics>::value,
              "type is not trivial");
