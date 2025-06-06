// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "StartPoint.hpp"
#include "Task/Ordered/Settings.hpp"
#include "Task/ObservationZones/Boundary.hpp"
#include "Task/TaskBehaviour.hpp"
#include "Geo/Math.hpp"
#include "time/DateTime.hpp"

#include <cassert>

StartPoint::StartPoint(std::unique_ptr<ObservationZonePoint> &&_oz,
                       WaypointPtr &&wp,
                       const TaskBehaviour &tb,
                       const StartConstraints &_constraints)
  :OrderedTaskPoint(TaskPointType::START, std::move(_oz), std::move(wp), false),
   safety_height(tb.safety_height_arrival),
   margins(tb.start_margins),
   constraints(_constraints)
{
}

void
StartPoint::SetTaskBehaviour(const TaskBehaviour &tb) noexcept
{
  safety_height = tb.safety_height_arrival;
  margins = tb.start_margins;
}

double
StartPoint::GetElevation() const noexcept
{
  return GetBaseElevation() + safety_height;
}

void
StartPoint::SetOrderedTaskSettings(const OrderedTaskSettings &settings) noexcept
{
  OrderedTaskPoint::SetOrderedTaskSettings(settings);
  constraints = settings.start_constraints;
}

void
StartPoint::SetNeighbours(OrderedTaskPoint *_prev, OrderedTaskPoint *_next) noexcept
{
  assert(_prev==NULL);
  // should not ever have an inbound leg
  OrderedTaskPoint::SetNeighbours(_prev, _next);
}


void
StartPoint::find_best_start(const AircraftState &state,
                            const OrderedTaskPoint &next,
                            const FlatProjection &projection)
{
  /* check which boundary point results in the smallest distance to
     fly */

  const OZBoundary boundary = GetBoundary();
  assert(!boundary.empty());

  const auto end = boundary.end();
  auto i = boundary.begin();
  assert(i != end);

  const GeoPoint &next_location = next.GetLocationRemaining();

  GeoPoint best_location = *i;
  auto best_distance = ::DoubleDistance(state.location, *i, next_location);

  for (++i; i != end; ++i) {
    auto distance = ::DoubleDistance(state.location, *i, next_location);
    if (distance < best_distance) {
      best_location = *i;
      best_distance = distance;
    }
  }

  SetSearchMin(SearchPoint(best_location, projection));
}

bool
StartPoint::IsInSector(const AircraftState &state) const noexcept
{
  return OrderedTaskPoint::IsInSector(state) &&
    // TODO: not using margins?
    constraints.CheckHeight(state, GetBaseElevation());
}

bool
StartPoint::CheckExitTransition(const AircraftState &ref_now,
                                const AircraftState &ref_last) const noexcept
{
  if (!constraints.open_time_span.HasBegun(RoughTime{ref_last.time}))
    /* the start gate is not yet open when we left the OZ */
    return false;

  if (constraints.open_time_span.HasEnded(RoughTime{ref_now.time}))
    /* the start gate was already closed when we left the OZ */
    return false;
  auto now = DateTime::now();
  if (constraints.pev_open > now)
    return false;  // is defined and has not begun
  if (constraints.pev_closed > 0 && constraints.pev_closed < now)
    return false;  // is defined and has ended

  if (!constraints.CheckSpeed(ref_now.ground_speed, &margins) ||
      !constraints.CheckSpeed(ref_last.ground_speed, &margins))
    /* flying too fast */
    return false;

  // TODO: not using margins?
  const bool now_in_height =
    constraints.CheckHeight(ref_now, GetBaseElevation());
  const bool last_in_height =
    constraints.CheckHeight(ref_last, GetBaseElevation());

  if (now_in_height && last_in_height) {
    // both within height limit, so use normal location checks
    return OrderedTaskPoint::CheckExitTransition(ref_now, ref_last);
  }
  if (!TransitionConstraint(ref_now.location, ref_last.location)) {
    // don't allow vertical crossings for line OZ's
    return false;
  }

  // transition inside sector to above
  return !now_in_height && last_in_height
    && OrderedTaskPoint::IsInSector(ref_last)
    && CanStartThroughTop();
}
