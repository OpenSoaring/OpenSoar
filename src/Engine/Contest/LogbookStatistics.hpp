// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ContestResult.hpp"

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
   * Has the result been calculated exhaustively after the landing?
   * Only then is it final and the log book writes it.
   */
  bool final;

  constexpr void Reset() noexcept {
    free.Reset();
    dmst.Reset();
    dmst_shape = DMStShape::NONE;
    final = false;
  }
};

static_assert(std::is_trivial<LogbookStatistics>::value,
              "type is not trivial");
