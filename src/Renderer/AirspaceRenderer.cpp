// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "AirspaceRenderer.hpp"
#include "AirspaceRendererSettings.hpp"
#include "Projection/WindowProjection.hpp"
#include "Look/AirspaceLook.hpp"
#include "Airspace/Airspaces.hpp"
#include "Airspace/AirspaceVisibility.hpp"
#include "Airspace/AirspaceWarning.hpp"
#include "Airspace/ProtectedAirspaceWarningManager.hpp"
#include "Airspace/AirspaceWarningCopy.hpp"
#include "Engine/Airspace/AirspaceWarningManager.hpp"
#include "NMEA/Aircraft.hpp"

class AirspaceMapVisible
{
  const AirspaceVisibility visible_predicate;
  const AirspaceWarningCopy &warnings;

public:
  AirspaceMapVisible(const AirspaceComputerSettings &_computer_settings,
                     const AirspaceRendererSettings &_renderer_settings,
                     const AircraftState& _state,
                     const AirspaceWarningCopy& _warnings)
    :visible_predicate(_computer_settings, _renderer_settings, _state),
     warnings(_warnings) {}

  bool operator()(const AbstractAirspace& airspace) const {
    return visible_predicate(airspace) ||
      warnings.IsInside(airspace) ||
      warnings.HasWarning(airspace);
  }
};

void
AirspaceRenderer::DrawIntersections(Canvas &canvas,
                                    const WindowProjection &projection) const
{
  for (unsigned i = intersections.size(); i--;) {
    if (auto p = projection.GeoToScreenIfVisible(intersections[i]))
      look.intercept_icon.Draw(canvas, *p);
  }
}

void
AirspaceRenderer::Draw(Canvas &canvas,
#ifndef ENABLE_OPENGL
                       Canvas &stencil_canvas,
#endif
                       const WindowProjection &projection,
                       const AirspaceRendererSettings &settings,
                       const AirspaceWarningCopy &awc,
                       const AirspacePredicate &visible)
{
  if (airspaces == nullptr || airspaces->IsEmpty())
    return;

#ifdef ENABLE_OPENGL
  if (airspaces->GetSerial() != last_airspaces_serial ||
      awc.GetSerial() != last_warning_serial) {
    last_airspaces_serial = airspaces->GetSerial();
    last_warning_serial = awc.GetSerial();
    cache.Invalidate();
  }

  /* the padding fill needs a stencil buffer, which an off-screen
     buffer has only if the driver offers one */
  const auto action = OpenGL::render_buffer_stencil != GL_NONE
    ? cache.Check(projection,
                  MakeCacheKey(projection, settings, awc, visible))
    : MapLayerCache::Action::DIRECT;

  switch (action) {
  case MapLayerCache::Action::DIRECT:
    DrawInternal(canvas, projection, settings, awc, visible);
    break;

  case MapLayerCache::Action::RENDER:
    DrawInternal(cache.BeginRender(projection), cache.GetBufferProjection(),
                 settings, awc, visible);
    cache.EndRender();
    cache.Draw(projection);
    break;

  case MapLayerCache::Action::REUSE:
    cache.Draw(projection);
    break;
  }
#else
  DrawInternal(canvas,
               stencil_canvas,
               projection, settings, awc, visible);
#endif

  intersections = awc.GetLocations();
}

void
AirspaceRenderer::Draw(Canvas &canvas,
#ifndef ENABLE_OPENGL
                       Canvas &stencil_canvas,
#endif
                       const WindowProjection &projection,
                       const AirspaceRendererSettings &settings)
{
  if (airspaces == nullptr)
    return;

  AirspaceWarningCopy awc;
  if (warning_manager != nullptr)
    awc.Visit(*warning_manager);

  Draw(canvas,
#ifndef ENABLE_OPENGL
       stencil_canvas,
#endif
       projection, settings, awc, [](const auto &){ return true; });
}

void
AirspaceRenderer::Draw(Canvas &canvas,
#ifndef ENABLE_OPENGL
                       Canvas &stencil_canvas,
#endif
                       const WindowProjection &projection,
                       const MoreData &basic,
                       const DerivedInfo &calculated,
                       const AirspaceComputerSettings &computer_settings,
                       const AirspaceRendererSettings &settings)
{
  if (airspaces == nullptr)
    return;

  AirspaceWarningCopy awc;
  if (warning_manager != nullptr)
    awc.Visit(*warning_manager);

  const AircraftState aircraft = ToAircraftState(basic, calculated);
  const AirspaceMapVisible visible(computer_settings, settings,
                                   aircraft, awc);
  Draw(canvas,
#ifndef ENABLE_OPENGL
       stencil_canvas,
#endif
       projection, settings, awc, visible);
}

#ifdef ENABLE_OPENGL

/**
 * Everything besides the projection that decides how the airspaces
 * look: the settings, and which airspaces are visible with which
 * warning state.  Visibility depends on the altitude of the aircraft
 * and on activation times, so the set of visible airspaces near the
 * screen is part of the key, not only the warning serial.  The radius
 * covers the margin of the cached image.
 */
uint64_t
AirspaceRenderer::MakeCacheKey(const WindowProjection &projection,
                               const AirspaceRendererSettings &settings,
                               const AirspaceWarningCopy &awc,
                               const AirspacePredicate &visible) const noexcept
{
  using C = MapLayerCache;
  uint64_t key = C::KEY_START;

  key = C::Mix(key, settings.enable);
  key = C::Mix(key, settings.black_outline);
  key = C::Mix(key, unsigned(settings.altitude_mode));
  key = C::Mix(key, settings.clip_altitude);
  key = C::Mix(key, unsigned(settings.fill_mode));
  for (const auto &c : settings.classes) {
    key = C::Mix(key, c.display);
    key = C::Mix(key, (c.border_color.Red() << 16) |
                 (c.border_color.Green() << 8) | c.border_color.Blue());
    key = C::Mix(key, (c.fill_color.Red() << 16) |
                 (c.fill_color.Green() << 8) | c.fill_color.Blue());
    key = C::Mix(key, c.border_width);
    key = C::Mix(key, unsigned(c.fill_mode));
  }

  for (const auto &i :
         airspaces->QueryWithinRange(projection.GetGeoScreenCenter(),
                                     projection.GetScreenDistanceMeters() * 1.3)) {
    const AbstractAirspace &airspace = i.GetAirspace();
    if (!visible(airspace))
      continue;

    key = C::Mix(key, reinterpret_cast<uintptr_t>(&airspace));
    key = C::Mix(key, (awc.HasWarning(airspace) ? 1 : 0) |
                 (awc.IsInside(airspace) ? 2 : 0) |
                 (awc.IsAcked(airspace) ? 4 : 0));
  }

  return key;
}

#endif
