// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <cstdint>

/**
 * What the centre of the vario display page shows.
 */
enum class VarioDisplayCenterView : uint8_t {
  NONE,

  /** one wind arrow with the average wind as a tail vane */
  SINGLE_ARROW,

  /** two arrows: the wind and the average wind */
  DOUBLE_ARROW,

  /** the thermal assistant made of dots (circling only) */
  DOTTED_ASSISTANT,

  /** the thermal assistant as a spider's web (circling only) */
  SPIDER_ASSISTANT,

  MAX
};

/**
 * What an info line of the vario display page shows.  INFO1 is the
 * top row, INFO2 the bottom row; the wind entries only make sense
 * in the bottom row (two lines).
 */
enum class VarioDisplayLineView : uint8_t {
  NONE,
  AVG_CLIMB_RATE,
  BANK_ANGLE,
  BATTERY_VOLTAGE,
  CIRCLE_DIAMETER,
  CIRCLE_MAX_MIN,
  DRIFT_ANGLE,
  EQUIVALENT_AIRSPEED,
  FLIGHT_LEVEL,
  G_LOAD,
  HEADING,
  INDICATED_AIRSPEED,
  PITCH_ANGLE,
  SPEED_TO_FLY,
  TRUE_AIRSPEED,
  TRUE_COURSE,
  UTC_TIME,
  WIND_AND_AVG_WIND,
  WIND_AND_DELTA,

  MAX
};

/**
 * What the right hand margin of the vario display page shows.
 */
enum class VarioDisplayInfo3View : uint8_t {
  NONE,

  /** the climb rate averaged over the whole thermal */
  CLIMBING,

  SPEED_TO_FLY,

  MAX
};

/**
 * One set of views: what the vario display page shows in one flight
 * state.
 */
struct VarioDisplayViewSettings {
  VarioDisplayCenterView center;
  VarioDisplayLineView info1, info2;
  VarioDisplayInfo3View info3;

  constexpr bool operator==(const VarioDisplayViewSettings &) const noexcept = default;
};

/**
 * The configuration of the vario display page: one set of views
 * while circling, one in straight flight.  The switch between the
 * two follows the circling detection.
 */
struct VarioDisplaySettings {
  VarioDisplayViewSettings circling, straight;

  void SetDefaults() noexcept {
    circling.center = VarioDisplayCenterView::SINGLE_ARROW;
    circling.info1 = VarioDisplayLineView::NONE;
    circling.info2 = VarioDisplayLineView::WIND_AND_DELTA;
    circling.info3 = VarioDisplayInfo3View::CLIMBING;

    straight.center = VarioDisplayCenterView::SINGLE_ARROW;
    straight.info1 = VarioDisplayLineView::NONE;
    straight.info2 = VarioDisplayLineView::WIND_AND_DELTA;
    straight.info3 = VarioDisplayInfo3View::SPEED_TO_FLY;
  }
};
