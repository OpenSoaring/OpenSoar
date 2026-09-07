// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "VarioDisplayWindow.hpp"
#include "VarioDisplayGlyphs.hpp"
#include "NMEA/MoreData.hpp"
#include "Computer/Settings.hpp"
#include "MapSettings.hpp"
#include "UISettings.hpp"
#include "Interface.hpp"
#include "Units/Units.hpp"
#include "Look/FontDescription.hpp"
#include "Look/Look.hpp"
#include "Look/MapLook.hpp"
#include "UIGlobals.hpp"
#include "Language/Language.hpp"
#include "Renderer/FinalGlideBarRenderer.hpp"
#include "Dialogs/Settings/Panels/VarioDisplayConfigPanel.hpp"
#include "Input/InputEvents.hpp"
#include "Screen/Layout.hpp"
#include "ui/canvas/Canvas.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <span>

/*
 * The round vario display page.  Every proportion of the instrument
 * is normalized to the instrument radius R, so it scales to any
 * screen and centres itself.
 */

namespace {

/* the instrument palette */
constexpr Color VD_BACKGROUND = COLOR_BLACK;
constexpr Color VD_SCALE = COLOR_WHITE;
constexpr Color VD_NEEDLE{139, 0, 0};          // dark red
constexpr Color VD_AVG_CLIMB{0, 128, 0};       // green
constexpr Color VD_MC_CREADY{255, 0, 0};       // red
constexpr Color VD_STF_ARC{255, 165, 0};       // orange
constexpr Color VD_WIND_FILL{30, 144, 255};    // dodger blue
constexpr Color VD_WIND_STROKE = COLOR_WHITE;
constexpr Color VD_AVG_WIND_FILL{211, 211, 211};   // light gray
constexpr Color VD_AVG_WIND_STROKE{0, 0, 255};     // blue
constexpr Color VD_WIND_DIFF{255, 182, 193};       // light pink
constexpr Color VD_ICON{255, 165, 0};          // orange
constexpr Color VD_VALUE = COLOR_WHITE;
constexpr Color VD_UNIT{169, 169, 169};        // dark gray
constexpr Color VD_GO{50, 205, 50};            // lime green
constexpr Color VD_WARNING{255, 255, 0};       // yellow
constexpr Color VD_STOP{255, 0, 0};            // red

/* the thermal assistant colors: dotted and spider variant */
constexpr Color VD_TA_BEST{255, 255, 0};       // yellow
constexpr Color VD_TA_GOOD{255, 0, 0};         // red
constexpr Color VD_TA_BAD{0, 191, 255};        // deep sky blue
constexpr Color VD_TA_WORST = COLOR_BLACK;
constexpr Color VD_TA2_BAD{0, 0, 255};         // blue

/*
 * The pointer shapes: polar coordinates, alpha = 0 pointing up,
 * positive clockwise, len relative to the pointer's scale length.
 */
struct PolarCoord {
  double len, alpha;
};

/* the climb rate needle: a slim pointer riding on the scale ring */
constexpr PolarCoord CLASSIC_INDICATOR[] = {
  { 1.0, -0.0 },
  { 0.9335393173757407, 0.04051835247834694 },
  { 0.7026989039367824, 0.05384013197330247 },
  { 0.7026989039367824, -0.05384013197330247 },
  { 0.9335393173757407, -0.04051835247834694 },
};

/* the average climb marker: a triangle pointing outward */
constexpr PolarCoord SIMPLE_INDICATOR[] = {
  { 1.2228915662650603, -0.0 },
  { 1.0118509561103022, -0.10736992203554742 },
  { 1.0118509561103022, 0.10736992203554742 },
};

/* the MacCready marker: a triangle pointing inward from the rim */
constexpr PolarCoord SCALE_MARKER[] = {
  { 1.0143796901104953, -0.0582673029695334 },
  { 1.0143796901104953, 0.0582673029695334 },
  { 0.8776371308016877, -0.0 },
};

/* the wind arrow (tip at 0.66, two barbs) */
constexpr PolarCoord WIND_ARROW[] = {
  { 0.66, -0.0 },
  { 0.4220189569201838, 2.507569933926371 },
  { 0.0, 0.0 },
  { 0.4220189569201838, -2.507569933926371 },
};

/* the slim arrow of the double arrow view */
constexpr PolarCoord SLIM_ARROW[] = {
  { 0.5, -0.0 },
  { 0.3757658845611187, 0.4398425828157362 },
  { 0.34365680554879163, 0.1460122577112754 },
  { 0.5024937810560445, 3.0419240010986313 },
  { 0.5024937810560445, -3.0419240010986313 },
  { 0.34365680554879163, -0.1460122577112754 },
  { 0.3757658845611187, -0.4398425828157362 },
};

constexpr double NINE_O_CLOCK = 1.5 * M_PI;
constexpr double SIX_O_CLOCK = M_PI;

/**
 * A point at the given bearing (0 = up, clockwise) and radius.
 */
[[gnu::const]]
static PixelPoint
AtBearing(PixelPoint center, double radius, double bearing_rad) noexcept
{
  return {center.x + int(std::sin(bearing_rad) * radius),
          center.y - int(std::cos(bearing_rad) * radius)};
}

static void
DrawPolar(Canvas &canvas, std::span<const PolarCoord> coords,
          PixelPoint center, double scale, double rotation) noexcept
{
  BulkPixelPoint pts[8];
  unsigned n = 0;
  for (const auto &pc : coords)
    pts[n++] =
      BulkPixelPoint{AtBearing(center, pc.len * scale, pc.alpha + rotation)};

  canvas.DrawPolygon(pts, n);
}

/**
 * Paint a traced glyph centred on @p p, its longer side scaled to
 * @p size pixels.
 */
static void
DrawGlyph(Canvas &canvas, Glyph glyph,
          PixelPoint p, int size, Color fill, Color hole) noexcept
{
  BulkPixelPoint buf[80];

  canvas.SelectNullPen();
  for (const auto &contour : glyph) {
    const Brush brush{contour.hole ? hole : fill};
    canvas.Select(brush);

    unsigned n = 0;
    for (std::size_t i = 0; i < contour.n && n < std::size(buf); ++i, ++n)
      buf[n] = BulkPixelPoint{p.x + contour.pts[i].x * size / 1000,
                              p.y + contour.pts[i].y * size / 1000};
    canvas.DrawPolygon(buf, n);
  }
}

static void
DrawTextCentered(Canvas &canvas, PixelPoint p, const char *text) noexcept
{
  const PixelSize size = canvas.CalcTextSize(text);
  canvas.DrawText({p.x - int(size.width) / 2, p.y - int(size.height) / 2},
                  text);
}

/**
 * ft/min uses the "x 100" scale.
 */
[[gnu::pure]]
static double
ScaleDivisor() noexcept
{
  return Units::GetUserVerticalSpeedUnit() == Unit::FEET_PER_MINUTE
    ? 100.
    : 1.;
}

/**
 * Normalize an angle difference to -180..+180 degrees.
 */
[[gnu::const]]
static double
NormalizedDelta(double degrees) noexcept
{
  while (degrees > 180)
    degrees -= 360;
  while (degrees < -180)
    degrees += 360;
  return degrees;
}

/**
 * The ISA density ratio at the given pressure altitude, for the
 * equivalent airspeed.
 */
[[gnu::const]]
static double
DensityRatio(double pressure_altitude_m) noexcept
{
  const double x = 1 - 2.2558e-5 * pressure_altitude_m;
  return x > 0 ? std::pow(x, 4.2559) : 1;
}

} // anonymous namespace

void
VarioDisplayThermalData::Update(const VarioDisplayData &data) noexcept
{
  if (data.circling && !was_circling)
    Reset();
  was_circling = data.circling;

  if (!data.circling || !data.gross_vario_available ||
      !data.heading_available)
    return;

  const double alpha = data.heading.AsBearing().Radians();
  const unsigned idx =
    std::min(unsigned(alpha / (2 * M_PI) * COUNT), COUNT - 1);
  delta_climb[idx] = data.gross_vario - data.average_vario;
}

unsigned
VarioDisplayThermalData::GetBestIndex() const noexcept
{
  unsigned best = 0;
  for (unsigned i = 1; i < COUNT; ++i)
    if (delta_climb[i] > delta_climb[best])
      best = i;
  return best;
}

unsigned
VarioDisplayThermalData::GetWorstIndex() const noexcept
{
  unsigned worst = 0;
  for (unsigned i = 1; i < COUNT; ++i)
    if (delta_climb[i] < delta_climb[worst])
      worst = i;
  return worst;
}

double
VarioDisplayThermalData::GetMaxMin() const noexcept
{
  return delta_climb[GetBestIndex()] - delta_climb[GetWorstIndex()];
}

void
VarioDisplayWindow::ReadBlackboard(const MoreData &basic,
                                   const DerivedInfo &calculated,
                                   const ComputerSettings &settings,
                                   const MapSettings &map_settings,
                                   const UISettings &ui_settings,
                                   DisplayMode _display_mode) noexcept
{
  data.Clear();

  data.gross_vario_available = basic.brutto_vario_available;
  data.gross_vario = basic.brutto_vario;

  data.average_vario_available = basic.brutto_vario_available;
  data.average_vario = calculated.average;

  data.mc = settings.polar.glide_polar_task.GetMC();

  data.stf_available = calculated.V_stf_available;
  data.v_stf = calculated.V_stf;
  data.ias_available = basic.airspeed_available;
  data.ias = basic.indicated_airspeed;
  data.tas_available = basic.airspeed_available;
  data.tas = basic.true_airspeed;

  data.thermal_climb_available = calculated.current_thermal.IsDefined();
  data.thermal_climb = calculated.current_thermal.lift_rate;

  data.wind_available = calculated.wind_available;
  data.wind_bearing = calculated.wind.bearing;
  data.wind_speed = calculated.wind.norm;

  if (basic.external_wind_available) {
    data.inst_wind_available = true;
    data.inst_wind_bearing = basic.external_wind.bearing;
    data.inst_wind_speed = basic.external_wind.norm;
  } else if (data.wind_available) {
    data.inst_wind_available = true;
    data.inst_wind_bearing = data.wind_bearing;
    data.inst_wind_speed = data.wind_speed;
  }

  if (basic.attitude.heading_available) {
    data.heading = basic.attitude.heading;
    data.heading_available = true;
  } else if (basic.track_available) {
    data.heading = basic.track;
    data.heading_available = true;
  }

  data.track_available = basic.track_available;
  data.track = basic.track;

  data.bank_available = basic.attitude.bank_angle_available;
  data.bank_angle = basic.attitude.bank_angle;
  data.pitch_available = basic.attitude.pitch_angle_available;
  data.pitch_angle = basic.attitude.pitch_angle;

  data.g_available = basic.acceleration.available;
  data.g_load = basic.acceleration.g_load;

  data.voltage_available = basic.voltage_available;
  data.voltage = basic.voltage;

  data.pressure_altitude_available = basic.pressure_altitude_available;
  data.pressure_altitude = basic.pressure_altitude;

  data.turn_rate_available = calculated.turning;
  data.turn_rate =
    fabs(calculated.turn_rate_heading_smoothed.Degrees());
  data.turning_left = calculated.TurningLeft();

  data.utc_available = basic.time_available && basic.date_time_utc.IsPlausible();
  data.utc = basic.date_time_utc;

  data.circling = calculated.circling;

  data.connected = basic.alive;
  data.gps_ok = basic.location_available;
  data.simulator = basic.gps.simulator;
  data.replay = basic.gps.replay;

  thermal_data.Update(data);

  view_settings = ui_settings.vario_display;

  /* for the map overlays */
  derived = calculated;
  display_mode = _display_mode;
  final_glide_bar_mc0_enabled = map_settings.final_glide_bar_mc0_enabled;

  final_glide_bar_enabled =
    map_settings.final_glide_bar_display_mode != FinalGlideBarDisplayMode::OFF;
  if (map_settings.final_glide_bar_display_mode ==
      FinalGlideBarDisplayMode::AUTO) {
    /* the same validity rules as the map */
    const TaskStats &task_stats = calculated.task_stats;
    const GlideResult &solution = task_stats.total.solution_remaining;
    const GlideResult &solution_mc0 = task_stats.total.solution_mc0;
    const GlideSettings &glide_settings = settings.task.glide;

    if (!task_stats.task_valid || !solution.IsOk() ||
        !solution_mc0.IsDefined())
      final_glide_bar_enabled = false;
    else if (solution_mc0.SelectAltitudeDifference(glide_settings) < -1000 &&
             solution.SelectAltitudeDifference(glide_settings) < -1000)
      final_glide_bar_enabled = false;
  }

  Invalidate();
}

void
VarioDisplayWindow::UpdateFonts(unsigned radius) noexcept
{
  if (radius == font_radius || radius < 16)
    return;

  font_radius = radius;

  /* 40/30/25 px on a 480x480 display, i.e. relative to R=240 */
  scale_font.Load(FontDescription(std::max(radius * 40 / 240, 8u), true));
  big_font.Load(FontDescription(std::max(radius * 30 / 240, 8u), true));
  small_font.Load(FontDescription(std::max(radius * 25 / 240, 8u), true));
}

/**
 * The scale ring: a ring of 70/240 R width spanning 250 degrees
 * (the open side to the right), rounded ends, tick slits in the
 * outer part, the numbers and the unit centered on the ring.
 */
void
VarioDisplayWindow::DrawScale(Canvas &canvas, PixelPoint center, int radius,
                              double units_per_rev) const noexcept
{
  const unsigned r_out = radius;
  const unsigned ring_width = radius * 70 / 240;
  const unsigned r_in = r_out - ring_width;

  const Brush ring_brush{VD_SCALE};
  canvas.SelectNullPen();
  canvas.Select(ring_brush);

  /* the horseshoe: bearings 145..395 degrees, drawn in overlapping
     chunks so that no chunk crosses north */
  static constexpr double chunks[][2] = {
    { 145, 215 }, { 214, 284 }, { 283, 353 }, { 352, 360 }, { 0, 35 },
  };
  for (const auto &c : chunks)
    canvas.DrawAnnulus(center, r_in, r_out,
                       Angle::Degrees(c[0]), Angle::Degrees(c[1]));

  /* the rounded ends: discs of ring width on the middle radius */
  const double r_mid = (r_out + r_in) / 2.;
  canvas.DrawCircle(AtBearing(center, r_mid, Angle::Degrees(145).Radians()),
                    ring_width / 2);
  canvas.DrawCircle(AtBearing(center, r_mid, Angle::Degrees(35).Radians()),
                    ring_width / 2);

  /* tick slits: background colored lines of 10/240 R width over the
     outer 22/240 R of the ring; one slit per scale unit */
  const double degrees_per_unit = 250 / units_per_rev;
  const int num_ticks = int(units_per_rev) / 2;
  const unsigned tick_width = std::max(radius * 10 / 240, 2);
  const int tick_len = radius * 22 / 240;

  const Pen tick_pen{tick_width, VD_BACKGROUND};
  canvas.Select(tick_pen);
  for (int i = -num_ticks; i <= num_ticks; ++i) {
    const double bearing =
      Angle::Degrees(270 + i * degrees_per_unit).Radians();
    canvas.DrawLine(AtBearing(center, r_out + 1, bearing),
                    AtBearing(center, r_out - tick_len, bearing));
  }

  /* the numbers, black, centered on the middle of the ring */
  canvas.Select(scale_font);
  canvas.SetTextColor(VD_BACKGROUND);
  canvas.SetBackgroundTransparent();

  const int number_step = units_per_rev > 11 ? 2 : 1;
  char buffer[16];
  for (int i = -num_ticks; i <= num_ticks; i += number_step) {
    const double bearing =
      Angle::Degrees(270 + i * degrees_per_unit).Radians();
    snprintf(buffer, sizeof(buffer), "%d", std::abs(i));
    DrawTextCentered(canvas, AtBearing(center, r_mid, bearing), buffer);
  }

  /* the unit label, centered on the ring halfway between the two
     ticks below nine o'clock */
  canvas.Select(small_font);
  if (ScaleDivisor() > 1)
    snprintf(buffer, sizeof(buffer), "%sx100", Units::GetVerticalSpeedName());
  else
    snprintf(buffer, sizeof(buffer), "%s", Units::GetVerticalSpeedName());
  DrawTextCentered(canvas,
                   AtBearing(center, r_mid, Angle::Degrees(182.5).Radians()),
                   buffer);
}

/**
 * The arrow views in the instrument centre: the wind absolute with
 * a North mark while circling, relative to the heading with a
 * glider silhouette in straight flight.
 */
void
VarioDisplayWindow::DrawWindArrows(Canvas &canvas, PixelPoint center,
                                   int radius,
                                   bool double_arrow) const noexcept
{
  /* the North mark or the glider silhouette */
  if (data.circling)
    DrawGlyph(canvas, VD_GLYPH_NORTH,
              {center.x, center.y - radius * 202 / 240},
              radius * 48 / 240, VD_BACKGROUND, VD_SCALE);
  else
    DrawGlyph(canvas, VD_GLYPH_GLIDER,
              {center.x, center.y + radius * 12 / 240},
              radius * 222 / 240, VD_SCALE, VD_BACKGROUND);

  /* without any wind data the arrow still appears, pointing up at
     its minimum length, so the configured view is visible */
  const bool have_wind = data.inst_wind_available;

  const auto relative = [&](Angle bearing) {
    double b = bearing.Radians();
    if (!data.circling && data.heading_available)
      b -= data.heading.Radians();
    /* below 30 km/h IAS the relative direction is meaningless:
       point straight up */
    if (data.ias_available && data.ias < 30 / 3.6)
      b = M_PI;
    return b;
  };

  /* 10..30 km/h wind maps to 80..150 px at R=240 */
  const auto arrow_len = [&](double speed) {
    const double kmh = speed * 3.6;
    const int len_min = radius * 80 / 240, len_max = radius * 150 / 240;
    if (kmh <= 10)
      return len_min;
    if (kmh >= 30)
      return len_max;
    return len_min + int((len_max - len_min) * (kmh - 10) / 20);
  };

  const double live_speed = have_wind ? data.inst_wind_speed : 0;
  const double live_bearing_rad = have_wind
    ? relative(data.inst_wind_bearing)
    : M_PI;

  if (double_arrow) {
    /* the average wind: a slim gray arrow below the live wind */
    if (data.wind_available) {
      const Brush avg_fill{VD_AVG_WIND_FILL};
      canvas.Select(avg_fill);
      canvas.SelectNullPen();
      DrawPolar(canvas, SLIM_ARROW, center, arrow_len(data.wind_speed) * 2,
                SIX_O_CLOCK + relative(data.wind_bearing));
    }

    const Brush fill{VD_WIND_FILL};
    const Pen stroke{1, VD_AVG_WIND_STROKE};
    canvas.Select(fill);
    canvas.Select(stroke);
    DrawPolar(canvas, SLIM_ARROW, center,
              arrow_len(live_speed) * 2,
              SIX_O_CLOCK + live_bearing_rad);
  } else {
    const Brush fill{VD_WIND_FILL};
    const Pen stroke{2, VD_WIND_STROKE};
    canvas.Select(fill);
    canvas.Select(stroke);
    DrawPolar(canvas, WIND_ARROW, center,
              arrow_len(live_speed),
              SIX_O_CLOCK + live_bearing_rad);

    /* the tail: the angle to the average wind, thickness by the
       speed difference */
    if (have_wind && data.wind_available) {
      const double tail = NormalizedDelta((data.wind_bearing -
                                           data.inst_wind_bearing).Degrees());
      if (std::fabs(tail) >= 2) {
        const double tip_len =
          0.66 * arrow_len(data.inst_wind_speed);
        const double delta_kmh =
          std::fabs(data.inst_wind_speed - data.wind_speed) * 3.6;
        const unsigned thick =
          std::clamp(unsigned(delta_kmh), 2u, 10u) * radius / 240;
        const double tip = SIX_O_CLOCK + relative(data.inst_wind_bearing);

        const Pen pen{std::max(thick, 2u), VD_WIND_DIFF};
        canvas.SelectHollowBrush();
        canvas.Select(pen);
        const double a0 = tip * 180 / M_PI, a1 = a0 + tail;
        canvas.DrawArc(center, unsigned(tip_len),
                       Angle::Degrees(std::min(a0, a1)),
                       Angle::Degrees(std::max(a0, a1)));
      }
    }
  }
}

/**
 * The thermal assistant views: 24 samples around the circle, as
 * dots or as a spider's web, rotating with the heading; the small
 * glider sits at the side the pilot flies on.
 */
void
VarioDisplayWindow::DrawThermalAssistant(Canvas &canvas, PixelPoint center,
                                         int radius,
                                         bool spider) const noexcept
{
  const double ta_radius = radius * 65 / 240.;
  const unsigned best = thermal_data.GetBestIndex();
  const unsigned worst = thermal_data.GetWorstIndex();

  double rotation = -data.heading.AsBearing().Radians();
  rotation += data.turning_left ? NINE_O_CLOCK : M_PI / 2;

  const auto color_of = [&](unsigned i) {
    if (i == best)
      return VD_TA_BEST;
    if (i == worst)
      return VD_TA_WORST;
    if (thermal_data.Get(i) > 0)
      return VD_TA_GOOD;
    return spider ? VD_TA2_BAD : VD_TA_BAD;
  };

  const double delta = 2 * M_PI / VarioDisplayThermalData::COUNT;

  if (spider) {
    PixelPoint prev{}, first{};
    Color first_color{};
    bool have_prev = false;

    const Pen web_pen{1, VD_SCALE};
    canvas.SelectNullPen();

    for (unsigned i = 0; i <= VarioDisplayThermalData::COUNT; ++i) {
      const unsigned idx = i % VarioDisplayThermalData::COUNT;
      const double v = thermal_data.Get(idx);
      const double scale =
        std::clamp((v + 3) * 0.15 + 0.4, 0.4, 1.3) * ta_radius;
      const PixelPoint p = AtBearing(center, scale, idx * delta + rotation);

      if (have_prev) {
        const Color c = color_of(idx == 0 ? 0 : idx);
        const Brush brush{i == VarioDisplayThermalData::COUNT
                          ? first_color : c};
        canvas.Select(brush);
        const BulkPixelPoint tri[] = {
          BulkPixelPoint{center}, BulkPixelPoint{prev}, BulkPixelPoint{p},
        };
        canvas.SelectNullPen();
        canvas.DrawPolygon(tri, 3);

        canvas.Select(web_pen);
        canvas.SelectHollowBrush();
        canvas.DrawLine(prev, p);
        canvas.SelectNullPen();
      } else {
        first = p;
        first_color = color_of(idx);
      }
      prev = p;
      have_prev = true;
    }
  } else {
    canvas.SelectNullPen();
    for (unsigned i = 0; i < VarioDisplayThermalData::COUNT; ++i) {
      const double v = thermal_data.Get(i);
      const unsigned diameter =
        std::clamp(unsigned(std::fabs(v) * 10 * radius / 240),
                   unsigned(radius * 25 / 240 / 3),
                   unsigned(radius * 25 / 240));
      const Brush brush{color_of(i)};
      canvas.Select(brush);
      canvas.DrawCircle(AtBearing(center, ta_radius, i * delta + rotation),
                        std::max(diameter / 2, 2u));
    }
  }

  /* the small glider at the circle's edge */
  const int glider_w = radius * 70 / 240;
  const int dx = data.turning_left
    ? -int(ta_radius) - glider_w / 2
    : int(ta_radius) + glider_w / 2;
  DrawGlyph(canvas, VD_GLYPH_SMALL_GLIDER,
            {center.x + dx, center.y},
            glider_w, VD_SCALE, VD_BACKGROUND);
}

void
VarioDisplayWindow::DrawCenterView(Canvas &canvas, PixelPoint center,
                                   int radius,
                                   VarioDisplayCenterView view) const noexcept
{
  switch (view) {
  case VarioDisplayCenterView::NONE:
  case VarioDisplayCenterView::MAX:
    break;

  case VarioDisplayCenterView::SINGLE_ARROW:
    DrawWindArrows(canvas, center, radius, false);
    break;

  case VarioDisplayCenterView::DOUBLE_ARROW:
    DrawWindArrows(canvas, center, radius, true);
    break;

  case VarioDisplayCenterView::DOTTED_ASSISTANT:
    DrawThermalAssistant(canvas, center, radius, false);
    break;

  case VarioDisplayCenterView::SPIDER_ASSISTANT:
    DrawThermalAssistant(canvas, center, radius, true);
    break;
  }
}

/**
 * One info line: an icon, a value and an optional unit, centered on
 * @p pos like the original's info rows.
 */
void
VarioDisplayWindow::DrawLineView(Canvas &canvas, PixelPoint pos, int radius,
                                 VarioDisplayLineView view) const noexcept
{
  char value[32] = "--";
  bool invalid = false;
  const char *unit = nullptr;
  Glyph icon{};
  unsigned icon_w1000 = 1000, icon_h1000 = 1000;

  const auto set_icon = [&](Glyph g, unsigned w, unsigned h) {
    icon = g;
    icon_w1000 = w;
    icon_h1000 = h;
  };

  /* the two line wind views are handled separately below */
  const bool wind_view = view == VarioDisplayLineView::WIND_AND_AVG_WIND ||
    view == VarioDisplayLineView::WIND_AND_DELTA;

  if (!wind_view) {
    switch (view) {
    case VarioDisplayLineView::AVG_CLIMB_RATE:
      if (!data.average_vario_available)
        invalid = true;
      if (!invalid) snprintf(value, sizeof(value), "%.1f",
               Units::ToUserVSpeed(data.average_vario));
      unit = Units::GetVerticalSpeedName();
      set_icon(VD_GLYPH_AVG_CLIMB,
               VD_GLYPH_AVG_CLIMB_WIDTH, VD_GLYPH_AVG_CLIMB_HEIGHT);
      break;

    case VarioDisplayLineView::BANK_ANGLE: {
      if (!data.bank_available)
        invalid = true;
      const double deg = data.bank_angle.Degrees();
      if (!invalid) snprintf(value, sizeof(value), "%.0f\xC2\xB0", std::fabs(deg));
      if (deg < 0)
        set_icon(VD_GLYPH_ROLL_LEFT,
                 VD_GLYPH_ROLL_LEFT_WIDTH, VD_GLYPH_ROLL_LEFT_HEIGHT);
      else
        set_icon(VD_GLYPH_ROLL_RIGHT,
                 VD_GLYPH_ROLL_RIGHT_WIDTH, VD_GLYPH_ROLL_RIGHT_HEIGHT);
      break;
    }

    case VarioDisplayLineView::BATTERY_VOLTAGE:
      if (!data.voltage_available)
        invalid = true;
      if (!invalid) snprintf(value, sizeof(value), "%.1f", data.voltage);
      unit = "V";
      set_icon(VD_GLYPH_BATTERY,
               VD_GLYPH_BATTERY_WIDTH, VD_GLYPH_BATTERY_HEIGHT);
      break;

    case VarioDisplayLineView::CIRCLE_DIAMETER: {
      if (!data.turn_rate_available || !data.tas_available ||
          data.turn_rate < 1)
        invalid = true;
      const double diameter_m =
        2 * data.tas / (data.turn_rate * M_PI / 180);
      if (!invalid) snprintf(value, sizeof(value), "%.0f",
               Units::ToUserDistance(diameter_m) *
               (Units::GetUserDistanceUnit() == Unit::KILOMETER ? 1000 : 1));
      unit = Units::GetUserDistanceUnit() == Unit::KILOMETER
        ? "m" : Units::GetDistanceName();
      set_icon(VD_GLYPH_CIRCLE_DIAMETER,
               VD_GLYPH_CIRCLE_DIAMETER_WIDTH,
               VD_GLYPH_CIRCLE_DIAMETER_HEIGHT);
      break;
    }

    case VarioDisplayLineView::CIRCLE_MAX_MIN:
      if (!data.circling)
        invalid = true;
      if (!invalid) snprintf(value, sizeof(value), "%.1f",
               Units::ToUserVSpeed(thermal_data.GetMaxMin()));
      unit = Units::GetVerticalSpeedName();
      set_icon(VD_GLYPH_CIRCLE_DELTA,
               VD_GLYPH_CIRCLE_DELTA_WIDTH, VD_GLYPH_CIRCLE_DELTA_HEIGHT);
      break;

    case VarioDisplayLineView::DRIFT_ANGLE: {
      if (!data.track_available || !data.heading_available)
        invalid = true;
      const double drift =
        NormalizedDelta((data.track - data.heading).Degrees());
      if (!invalid) snprintf(value, sizeof(value), "%+.0f\xC2\xB0", drift);
      set_icon(VD_GLYPH_DRIFT_ANGLE,
               VD_GLYPH_DRIFT_ANGLE_WIDTH, VD_GLYPH_DRIFT_ANGLE_HEIGHT);
      break;
    }

    case VarioDisplayLineView::EQUIVALENT_AIRSPEED:
      if (!data.tas_available || !data.pressure_altitude_available)
        invalid = true;
      if (!invalid) snprintf(value, sizeof(value), "%.0f",
               Units::ToUserSpeed(data.tas *
                                  std::sqrt(DensityRatio(data.pressure_altitude))));
      unit = Units::GetSpeedName();
      set_icon(VD_GLYPH_EAS, VD_GLYPH_EAS_WIDTH, VD_GLYPH_EAS_HEIGHT);
      break;

    case VarioDisplayLineView::FLIGHT_LEVEL:
      if (!data.pressure_altitude_available)
        invalid = true;
      if (!invalid) snprintf(value, sizeof(value), "%03.0f",
               std::max(data.pressure_altitude / 0.3048 / 100, 0.));
      set_icon(VD_GLYPH_FLIGHT_LEVEL,
               VD_GLYPH_FLIGHT_LEVEL_WIDTH, VD_GLYPH_FLIGHT_LEVEL_HEIGHT);
      break;

    case VarioDisplayLineView::G_LOAD:
      if (!data.g_available)
        invalid = true;
      if (!invalid) snprintf(value, sizeof(value), "%.1f", data.g_load);
      unit = "g";
      set_icon(VD_GLYPH_G_LOAD,
               VD_GLYPH_G_LOAD_WIDTH, VD_GLYPH_G_LOAD_HEIGHT);
      break;

    case VarioDisplayLineView::HEADING:
      if (!data.heading_available)
        invalid = true;
      if (!invalid) snprintf(value, sizeof(value), "%.0f\xC2\xB0",
               data.heading.AsBearing().Degrees());
      set_icon(VD_GLYPH_HEADING,
               VD_GLYPH_HEADING_WIDTH, VD_GLYPH_HEADING_HEIGHT);
      break;

    case VarioDisplayLineView::INDICATED_AIRSPEED:
      if (!data.ias_available)
        invalid = true;
      if (!invalid) snprintf(value, sizeof(value), "%.0f", Units::ToUserSpeed(data.ias));
      unit = Units::GetSpeedName();
      set_icon(VD_GLYPH_IAS, VD_GLYPH_IAS_WIDTH, VD_GLYPH_IAS_HEIGHT);
      break;

    case VarioDisplayLineView::PITCH_ANGLE:
      if (!data.pitch_available)
        invalid = true;
      if (!invalid) snprintf(value, sizeof(value), "%+.0f\xC2\xB0",
               data.pitch_angle.Degrees());
      set_icon(VD_GLYPH_PITCH,
               VD_GLYPH_PITCH_WIDTH, VD_GLYPH_PITCH_HEIGHT);
      break;

    case VarioDisplayLineView::SPEED_TO_FLY:
      if (!data.stf_available)
        invalid = true;
      if (!invalid) snprintf(value, sizeof(value), "%.0f", Units::ToUserSpeed(data.v_stf));
      unit = Units::GetSpeedName();
      set_icon(VD_GLYPH_SPEED_TO_FLY,
               VD_GLYPH_SPEED_TO_FLY_WIDTH, VD_GLYPH_SPEED_TO_FLY_HEIGHT);
      break;

    case VarioDisplayLineView::TRUE_AIRSPEED:
      if (!data.tas_available)
        invalid = true;
      if (!invalid) snprintf(value, sizeof(value), "%.0f", Units::ToUserSpeed(data.tas));
      unit = Units::GetSpeedName();
      set_icon(VD_GLYPH_TAS, VD_GLYPH_TAS_WIDTH, VD_GLYPH_TAS_HEIGHT);
      break;

    case VarioDisplayLineView::TRUE_COURSE:
      if (!data.track_available)
        invalid = true;
      if (!invalid) snprintf(value, sizeof(value), "%.0f\xC2\xB0",
               data.track.AsBearing().Degrees());
      set_icon(VD_GLYPH_TRUE_COURSE,
               VD_GLYPH_TRUE_COURSE_WIDTH, VD_GLYPH_TRUE_COURSE_HEIGHT);
      break;

    case VarioDisplayLineView::UTC_TIME:
      if (!data.utc_available)
        invalid = true;
      if (!invalid) snprintf(value, sizeof(value), "%02u:%02u:%02u",
               data.utc.hour, data.utc.minute, data.utc.second);
      break;

    case VarioDisplayLineView::NONE:
    case VarioDisplayLineView::WIND_AND_AVG_WIND:
    case VarioDisplayLineView::WIND_AND_DELTA:
    case VarioDisplayLineView::MAX:
      return;
    }

    /* icon + value + unit, the text centered like the original */
    canvas.SetBackgroundTransparent();
    canvas.Select(big_font);

    const int font_h = (int)big_font.GetHeight();
    const int icon_h = font_h * 5 / 4;
    const int icon_w = !icon.empty() ? icon_h * int(icon_w1000) / int(icon_h1000) : 0;

    int unit_w = 0;
    if (unit != nullptr) {
      canvas.Select(small_font);
      unit_w = (int)canvas.CalcTextSize(unit).width + radius * 4 / 240;
      canvas.Select(big_font);
    }

    const int text_x = pos.x + icon_w / 2 - unit_w / 2;
    canvas.SetTextColor(VD_VALUE);
    DrawTextCentered(canvas, {text_x, pos.y}, value);

    const PixelSize text_size = canvas.CalcTextSize(value);

    if (!icon.empty()) {
      const int max_dim = std::max(icon_w, icon_h);
      DrawGlyph(canvas, icon,
                {text_x - int(text_size.width) / 2 - icon_w / 2
                 - radius * 4 / 240, pos.y},
                max_dim, VD_ICON, VD_BACKGROUND);
    }

    if (unit != nullptr) {
      canvas.Select(small_font);
      canvas.SetTextColor(VD_UNIT);
      canvas.DrawText({text_x + int(text_size.width) / 2 + radius * 4 / 240,
                       pos.y - (int)small_font.GetHeight() / 2},
                      unit);
    }

    return;
  }

  /* the two line wind views */
  canvas.SetBackgroundTransparent();

  if (!data.inst_wind_available) {
    canvas.Select(big_font);
    canvas.SetTextColor(VD_VALUE);
    DrawTextCentered(canvas, pos, "--");
    return;
  }

  const int big_h = (int)big_font.GetHeight();
  const int small_h = (int)small_font.GetHeight();
  const int total_h = (big_h + small_h) * 8 / 10;

  const double user_speed = Units::ToUserSpeed(data.inst_wind_speed);
  const double degrees = user_speed < 0.5
    ? 0 : data.inst_wind_bearing.AsBearing().Degrees();
  snprintf(value, sizeof(value), "%.0f\xC2\xB0 %.0f", degrees, user_speed);

  canvas.Select(big_font);
  canvas.SetTextColor(VD_VALUE);
  const PixelPoint top{pos.x, pos.y - total_h / 2};
  DrawTextCentered(canvas, top, value);

  const PixelSize size = canvas.CalcTextSize(value);
  canvas.Select(small_font);
  canvas.SetTextColor(VD_UNIT);
  canvas.DrawText({pos.x + int(size.width) / 2 + radius * 4 / 240,
                   top.y - small_h / 2},
                  Units::GetSpeedName());

  /* the second line: the average wind, or the delta to it */
  if (!data.wind_available)
    return;

  char second[32];
  if (view == VarioDisplayLineView::WIND_AND_AVG_WIND) {
    const double avg_user = Units::ToUserSpeed(data.wind_speed);
    snprintf(second, sizeof(second), "%.0f\xC2\xB0 %.0f",
             avg_user < 0.5 ? 0 : data.wind_bearing.AsBearing().Degrees(),
             avg_user);
  } else {
    /* the vector difference between the live and the average wind */
    const auto [is, ic] = data.inst_wind_bearing.SinCos();
    const auto [as, ac] = data.wind_bearing.SinCos();
    const double dx = data.inst_wind_speed * is - data.wind_speed * as;
    const double dy = data.inst_wind_speed * ic - data.wind_speed * ac;
    snprintf(second, sizeof(second), "%+.0f\xC2\xB0 %.0f",
             NormalizedDelta((data.inst_wind_bearing -
                              data.wind_bearing).Degrees()),
             Units::ToUserSpeed(std::hypot(dx, dy)));
  }

  canvas.SetTextColor(VD_WIND_DIFF);
  DrawTextCentered(canvas, {pos.x, pos.y + total_h / 2}, second);
}

/**
 * The right hand margin: the thermal climb rate or the speed to
 * fly, with the matching mode icon.
 */
void
VarioDisplayWindow::DrawInfo3(Canvas &canvas, PixelPoint center, int radius,
                              VarioDisplayInfo3View view) const noexcept
{
  char buffer[32] = "--";
  bool stf = false;

  switch (view) {
  case VarioDisplayInfo3View::CLIMBING:
    if (data.thermal_climb_available)
      snprintf(buffer, sizeof(buffer), "%.1f",
               Units::ToUserVSpeed(data.thermal_climb));
    break;

  case VarioDisplayInfo3View::SPEED_TO_FLY:
    stf = true;
    if (data.stf_available)
      snprintf(buffer, sizeof(buffer), "%.0f",
               Units::ToUserSpeed(data.v_stf));
    break;

  case VarioDisplayInfo3View::NONE:
  case VarioDisplayInfo3View::MAX:
    return;
  }

  canvas.SetBackgroundTransparent();

  const PixelPoint info3{center.x + radius * 170 / 240,
                         center.y + radius * 40 / 240};

  canvas.Select(big_font);
  canvas.SetTextColor(VD_SCALE);
  const PixelSize size = canvas.CalcTextSize(buffer);
  canvas.DrawText({info3.x - int(size.width), info3.y}, buffer);

  canvas.Select(small_font);
  canvas.SetTextColor(VD_UNIT);
  canvas.DrawText({info3.x - int(size.width), info3.y + int(size.height)},
                  stf ? Units::GetSpeedName()
                      : Units::GetVerticalSpeedName());

  /* the mode icon above the value */
  const PixelPoint icon{center.x + radius * 140 / 240,
                        center.y - radius * 15 / 240};
  const int icon_s = std::max(radius * 40 / 240, 10);
  DrawGlyph(canvas, stf ? Glyph{VD_GLYPH_STRAIGHT} : Glyph{VD_GLYPH_SPIRAL},
            icon, icon_s, VD_ICON, VD_BACKGROUND);
}

/**
 * The satellite symbol: green with a GPS fix, yellow while waiting
 * for the fix, red without a data connection.
 */
void
VarioDisplayWindow::DrawSatSymbol(Canvas &canvas, PixelPoint center,
                                  int radius) const noexcept
{
  const Color color = !data.connected
    ? VD_STOP
    : (data.gps_ok ? VD_GO : VD_WARNING);

  DrawGlyph(canvas, VD_GLYPH_SAT,
            {center.x + radius * 168 / 240,
             center.y - radius * 105 / 240},
            std::max(radius * 42 / 240, 10), color, VD_BACKGROUND);
}

/**
 * The orange speed command arc: starting at nine o'clock, 2.5
 * degrees per km/h difference, on the 346/480 diameter.
 */
void
VarioDisplayWindow::DrawSpeedToFlyArc(Canvas &canvas, PixelPoint center,
                                      int radius) const noexcept
{
  if (!data.stf_available || !data.ias_available)
    return;

  /* difference = speed to fly - IAS [km/h] */
  const double dif_kmh = (data.v_stf - data.ias) * 3.6;
  const double sweep = 25 * std::clamp(-dif_kmh / 10, -5., 5.);
  if (std::fabs(sweep) < 1)
    return;

  const unsigned r_mid = radius * 173 / 240;
  const unsigned half_width = std::max(radius * 5 / 240, 2);

  const Brush brush{VD_STF_ARC};
  canvas.SelectNullPen();
  canvas.Select(brush);
  if (sweep > 0)
    canvas.DrawAnnulus(center, r_mid - half_width, r_mid + half_width,
                       Angle::Degrees(270), Angle::Degrees(270 + sweep));
  else
    canvas.DrawAnnulus(center, r_mid - half_width, r_mid + half_width,
                       Angle::Degrees(270 + sweep), Angle::Degrees(270));
}

/**
 * MacCready marker, average climb marker and the climb needle, all
 * rotating around nine o'clock.
 */
void
VarioDisplayWindow::DrawNeedles(Canvas &canvas, PixelPoint center, int radius,
                                double degrees_per_unit) const noexcept
{
  const double max_units = 125 / degrees_per_unit;
  const double divisor = ScaleDivisor();

  const auto to_rotation = [&](double value_m_s, double limit_factor) {
    const double user = Units::ToUserVSpeed(value_m_s) / divisor;
    return std::clamp(user, -max_units * limit_factor,
                      max_units * limit_factor)
      * degrees_per_unit * M_PI / 180;
  };

  canvas.SelectNullPen();

  /* MacCready: red triangle pointing inward from the rim */
  const Brush mc_brush{VD_MC_CREADY};
  canvas.Select(mc_brush);
  DrawPolar(canvas, SCALE_MARKER, center, radius * 238 / 240.,
            NINE_O_CLOCK + to_rotation(data.mc, 1.));

  /* the 30 s average: green triangle inside the ring */
  if (data.average_vario_available) {
    const Brush avg_brush{VD_AVG_CLIMB};
    canvas.Select(avg_brush);
    DrawPolar(canvas, SIMPLE_INDICATOR, center, radius * 167 / 240.,
              NINE_O_CLOCK + to_rotation(data.average_vario, 1.));
  }

  /* the climb needle */
  if (data.gross_vario_available) {
    const Brush needle_brush{VD_NEEDLE};
    canvas.Select(needle_brush);
    DrawPolar(canvas, CLASSIC_INDICATOR, center, radius * 238 / 240.,
              NINE_O_CLOCK + to_rotation(data.gross_vario, 1.02));
  }
}

/**
 * The elements known from the map: the final glide bar at the right
 * edge, the flight mode icon at the lower right and the status row
 * at the lower left.
 */
void
VarioDisplayWindow::DrawMapOverlays(Canvas &canvas, const PixelRect &rc,
                                    int status_height) const noexcept
{
  const Look &look = UIGlobals::GetLook();

  if (final_glide_bar_enabled) {
    const FinalGlideBarRenderer renderer{look.final_glide_bar,
                                         look.map.task};
    renderer.Draw(canvas, rc, derived,
                  CommonInterface::GetComputerSettings().task.glide,
                  final_glide_bar_mc0_enabled);
  }

  /* the flight mode icon at the lower right, like the map, but
     never smaller than about 7 mm */
  {
    const MaskedIcon *bmp;
    if (derived.common_stats.task_type == TaskType::ABORT)
      bmp = &look.map.abort_mode_icon;
    else if (display_mode == DisplayMode::CIRCLING)
      bmp = &look.map.climb_mode_icon;
    else if (display_mode == DisplayMode::FINAL_GLIDE)
      bmp = &look.map.final_glide_mode_icon;
    else
      bmp = &look.map.cruise_mode_icon;

    const unsigned target_height =
      std::max(bmp->GetSize().height, Layout::vdpi * 7 / 25);
    const PixelSize size = bmp->GetScaledSize(target_height);
    bmp->Draw(canvas,
              PixelPoint(rc.right - int(size.width) - Layout::Scale(6),
                         rc.bottom - int(size.height) - Layout::Scale(2)),
              target_height);
  }

  /* the status row at the lower left: the GPS state, or the data
     source when it is not a real fix */
  const char *txt = nullptr;
  const MaskedIcon *icon = nullptr;
  if (!data.connected) {
    icon = &look.map.no_gps_icon;
    txt = _("GPS not connected");
  } else if (!data.gps_ok) {
    icon = &look.map.waiting_for_fix_icon;
    txt = _("GPS waiting for fix");
  } else if (data.simulator) {
    txt = _("Simulator");
  } else if (data.replay) {
    txt = _("Replay");
  }

  if (txt == nullptr)
    return;

  canvas.Select(small_font);
  canvas.SetBackgroundTransparent();
  canvas.SetTextColor(VD_VALUE);

  PixelPoint p(rc.left + Layout::FastScale(2),
               rc.bottom - (status_height + (int)small_font.GetHeight()) / 2);
  if (icon != nullptr) {
    /* scale the icon to the text height - the density variant can
       be much smaller */
    const unsigned target_height =
      std::max(icon->GetSize().height, small_font.GetHeight());
    const PixelSize size = icon->GetScaledSize(target_height);
    icon->Draw(canvas, {p.x, rc.bottom - status_height / 2
                        - (int)size.height / 2},
               target_height);
    p.x += size.width + Layout::FastScale(4);
  }
  canvas.DrawText(p, txt);
}

bool
VarioDisplayWindow::OnMouseDouble([[maybe_unused]] PixelPoint p) noexcept
{
  StopDragging();
  InputEvents::ShowMenu();
  return true;
}

bool
VarioDisplayWindow::OnMouseDown(PixelPoint p) noexcept
{
  if (!dragging) {
    dragging = true;
    SetCapture();
    gestures.Start(p, Layout::Scale(20));
  }

  return true;
}

bool
VarioDisplayWindow::OnMouseUp([[maybe_unused]] PixelPoint p) noexcept
{
  if (dragging) {
    StopDragging();

    const char *gesture = gestures.Finish();
    if (gesture && InputEvents::processGesture(gesture))
      return true;

    /* a plain tap opens the page's settings */
    ShowVarioDisplayConfigDialog();
    return true;
  }

  return false;
}

bool
VarioDisplayWindow::OnMouseMove(PixelPoint p,
                                [[maybe_unused]] unsigned keys) noexcept
{
  if (dragging)
    gestures.Update(p);

  return true;
}

void
VarioDisplayWindow::OnCancelMode() noexcept
{
  AntiFlickerWindow::OnCancelMode();
  StopDragging();
}

bool
VarioDisplayWindow::OnKeyDown(unsigned key_code) noexcept
{
  return InputEvents::processKey(key_code);
}

void
VarioDisplayWindow::OnPaintBuffer(Canvas &canvas) noexcept
{
  canvas.Clear(VD_BACKGROUND);

  const PixelRect rc = canvas.GetRect();

  /* leave room over the instrument (at least 5 mm) and exactly one
     status row (about 6 mm) below it; the instrument is pinned to
     the status row, spare height enlarges the top gap */
  const int mm_px = std::max(int(Layout::vdpi) * 10 / 254, 2);
  const int top_margin = 5 * mm_px;
  const int status_height = 6 * mm_px;

  const int radius =
    std::min(int(rc.GetWidth()) / 2 - 2,
             (int(rc.GetHeight()) - top_margin - status_height) / 2 - 2);
  if (radius < 16)
    return;

  const PixelPoint center{rc.GetCenter().x,
                          rc.bottom - status_height - 2 - radius};

  UpdateFonts(radius);

  /* the scale: +-5 units for m/s, +-10 for knots and ft/min x 100 */
  const bool is_ten =
    Units::GetUserVerticalSpeedUnit() != Unit::METER_PER_SECOND;
  const double units_per_rev = is_ten ? 20 : 10;
  const double degrees_per_unit = is_ten ? 12.5 : 25;

  DrawScale(canvas, center, radius, units_per_rev);

  /* which of the two view sets applies */
  const VarioDisplayViewSettings &views = data.circling
    ? view_settings.circling
    : view_settings.straight;

  DrawCenterView(canvas, center, radius, views.center);
  DrawLineView(canvas, {center.x, center.y - radius * 120 / 240},
               radius, views.info1);
  DrawLineView(canvas, {center.x, center.y + radius * 120 / 240},
               radius, views.info2);
  DrawInfo3(canvas, center, radius, views.info3);
  DrawSatSymbol(canvas, center, radius);

  if (!data.circling)
    DrawSpeedToFlyArc(canvas, center, radius);

  DrawNeedles(canvas, center, radius, degrees_per_unit);

  DrawMapOverlays(canvas, rc, status_height);
}
