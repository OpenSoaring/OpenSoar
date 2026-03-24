// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "MapWindowProjection.hpp"
#include "Screen/Layout.hpp"
#include "Waypoint/Waypoint.hpp"

#ifdef ENABLE_OPENGL
#include "ui/canvas/opengl/Globals.hpp"
#endif

#include <algorithm> // for std::clamp()
#include <cassert>

static constexpr unsigned ScaleList[] = {
  100,           // 
  200,           // 2
  300,           // 1.5
  500,           // 1.67
  1000,          // 2
  2000,          // 2
  3000,          // 1.5

  5000,          // 1.67    =   2.5
  10000,         // 2       =   5
  20000,         // 2       =  10
  30000,         // 1.5     =  15
  50000,         // 1.67    =  25
  75000,         // 1.5     =  38 (37.5)
  100000,        // 1.33    =  50
  150000,        // 1.5     =  75
  200000,        // 1.33    = 100
  300000,        // 1.5     = 150
  500000,        // 1.67    = 250
  1000000,       // 2       = 500
};

#ifdef IS_OPENVARIO
// don't use the last scale value on OpenVario (Performance?)
  static constexpr unsigned ScaleListCount = std::size(ScaleList) - 1;
#else
  static constexpr unsigned ScaleListCount = std::size(ScaleList);
#endif

bool
MapWindowProjection::WaypointInScaleFilter(const Waypoint &way_point) const noexcept
{
  return (GetMapScale() <= (way_point.IsLandable() ? 20000 : 10000));
}

double
MapWindowProjection::CalculateMapScale(unsigned scale) const noexcept
{
  assert(scale < ScaleListCount);
  return double(ScaleList[scale]) *
    GetMapResolutionFactor() / Layout::Scale(GetScreenSize().width);
}

/**
 * Determine the effective number of usable entries in the ScaleList.
 * May be reduced by OpenGL::max_map_scale to work around GPU driver
 * bugs.
 */
static unsigned
EffectiveScaleListCount() noexcept
{
#ifdef ENABLE_OPENGL
  if (OpenGL::max_map_scale > 0) {
    for (unsigned i = 0; i < ScaleListCount; i++)
      if (ScaleList[i] > OpenGL::max_map_scale)
        return i;
  }
#endif

  return ScaleListCount;
}

double
MapWindowProjection::LimitMapScale(const double value) const noexcept
{
  return HaveScaleList() ? CalculateMapScale(FindMapScale(value)) : value;
}

double
MapWindowProjection::StepMapScale(const double scale, int Step) const noexcept
{
  int i = FindMapScale(scale) + Step;
  i = std::clamp(i, 0, (int)EffectiveScaleListCount() - 1);
  return CalculateMapScale(i);
}

unsigned
MapWindowProjection::FindMapScale(const double Value) const noexcept
{
  const unsigned effective_count = EffectiveScaleListCount();

  unsigned DesiredScale(Value * Layout::Scale(GetScreenSize().width)
                        / GetMapResolutionFactor());

  unsigned i;
  for (i = 0; i < effective_count; i++) {
    if (DesiredScale < ScaleList[i]) {
      if (i == 0)
        return 0;

      return i - (DesiredScale < (ScaleList[i] + ScaleList[i - 1]) / 2);
    }
  }

  return effective_count - 1;
}

void
MapWindowProjection::SetFreeMapScale(double x) noexcept
{
#ifdef ENABLE_OPENGL
  if (OpenGL::max_map_scale > 0)
    x = std::min(x, double(OpenGL::max_map_scale));
#endif

  SetScale(double(GetMapResolutionFactor()) / x);
}

void
MapWindowProjection::SetMapScale(const double x) noexcept
{
  SetScale(double(GetMapResolutionFactor()) / LimitMapScale(x));
}
