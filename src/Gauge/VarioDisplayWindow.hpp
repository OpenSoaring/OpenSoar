// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ui/window/AntiFlickerWindow.hpp"
#include "ui/canvas/Font.hpp"
#include "UIUtil/GestureManager.hpp"
#include "DisplayMode.hpp"
#include "VarioDisplaySettings.hpp"
#include "NMEA/Derived.hpp"
#include "time/BrokenDateTime.hpp"
#include "Math/Angle.hpp"

struct MoreData;
struct ComputerSettings;
struct MapSettings;
struct UISettings;

/**
 * The values shown by the vario display page, picked from the
 * blackboards.  All speeds are in m/s, all angles are bearings.
 */
struct VarioDisplayData {
  /** the needle: total-energy compensated ("brutto") vario */
  double gross_vario;
  bool gross_vario_available;

  /** the green marker: the 30 s average */
  double average_vario;
  bool average_vario_available;

  /** the red marker: MacCready */
  double mc;

  /** speed to fly and the airspeeds */
  double v_stf, ias, tas;
  bool stf_available, ias_available, tas_available;

  /** the climb rate of the current thermal */
  double thermal_climb;
  bool thermal_climb_available;

  /** the averaged wind (the wind estimate) */
  Angle wind_bearing;
  double wind_speed;
  bool wind_available;

  /** the live wind (from the device, when it delivers one) */
  Angle inst_wind_bearing;
  double inst_wind_speed;
  bool inst_wind_available;

  Angle heading;
  bool heading_available;

  Angle track;
  bool track_available;

  Angle bank_angle;
  bool bank_available;

  Angle pitch_angle;
  bool pitch_available;

  double g_load;
  bool g_available;

  double voltage;
  bool voltage_available;

  /** for the flight level */
  double pressure_altitude;
  bool pressure_altitude_available;

  /** degrees per second, for the circle diameter */
  double turn_rate;
  bool turn_rate_available;
  bool turning_left;

  BrokenTime utc;
  bool utc_available;

  bool circling;

  /** the satellite symbol: data link and GPS fix */
  bool connected, gps_ok;

  /** for the status row */
  bool simulator, replay;

  void Clear() noexcept {
    gross_vario_available = average_vario_available = false;
    stf_available = ias_available = tas_available = false;
    thermal_climb_available = false;
    wind_available = inst_wind_available = false;
    heading_available = track_available = false;
    bank_available = pitch_available = false;
    g_available = voltage_available = false;
    pressure_altitude_available = false;
    turn_rate_available = false;
    turning_left = false;
    utc_available = false;
    circling = false;
    connected = gps_ok = false;
    simulator = replay = false;
    gross_vario = average_vario = mc = 0;
    v_stf = ias = tas = thermal_climb = 0;
    wind_speed = inst_wind_speed = 0;
    g_load = voltage = pressure_altitude = turn_rate = 0;
  }
};

/**
 * The climb history around the circle, feeding the thermal
 * assistant views and the circle max-min read-out: 24 bins by
 * heading, each holding the climb rate relative to the average.
 */
class VarioDisplayThermalData {
public:
  static constexpr unsigned COUNT = 24;

private:
  double delta_climb[COUNT];
  bool was_circling = false;

public:
  VarioDisplayThermalData() noexcept {
    Reset();
  }

  void Reset() noexcept {
    for (auto &i : delta_climb)
      i = 0;
  }

  void Update(const VarioDisplayData &data) noexcept;

  [[gnu::pure]]
  double Get(unsigned i) const noexcept {
    return delta_climb[i];
  }

  [[gnu::pure]]
  unsigned GetBestIndex() const noexcept;

  [[gnu::pure]]
  unsigned GetWorstIndex() const noexcept;

  [[gnu::pure]]
  double GetMaxMin() const noexcept;
};

/**
 * A Window which renders a big round vario display as a full page:
 * the scale ring, the climb needle, the average and MacCready
 * markers, the configurable centre view and info lines, plus the
 * final glide bar, the flight mode icon and the status row.
 */
class VarioDisplayWindow : public AntiFlickerWindow {
  VarioDisplayData data;
  VarioDisplayThermalData thermal_data;

  VarioDisplaySettings view_settings;

  /** a copy for the final glide bar renderer */
  DerivedInfo derived;

  bool final_glide_bar_enabled = false;
  bool final_glide_bar_mc0_enabled = false;
  DisplayMode display_mode = DisplayMode::NONE;

  /** fonts scaled to the instrument radius */
  Font scale_font, big_font, small_font;
  unsigned font_radius = 0;

  GestureManager gestures;
  bool dragging = false;

public:
  VarioDisplayWindow() noexcept {
    data.Clear();
    view_settings.SetDefaults();
    derived.Reset();
  }

  void ReadBlackboard(const MoreData &basic, const DerivedInfo &calculated,
                      const ComputerSettings &settings,
                      const MapSettings &map_settings,
                      const UISettings &ui_settings,
                      DisplayMode _display_mode) noexcept;

protected:
  /* virtual methods from AntiFlickerWindow */
  void OnPaintBuffer(Canvas &canvas) noexcept override;

  /* virtual methods from Window */
  bool OnMouseDown(PixelPoint p) noexcept override;
  bool OnMouseUp(PixelPoint p) noexcept override;
  bool OnMouseMove(PixelPoint p, unsigned keys) noexcept override;
  bool OnMouseDouble(PixelPoint p) noexcept override;
  bool OnKeyDown(unsigned key_code) noexcept override;
  void OnCancelMode() noexcept override;

private:
  void StopDragging() noexcept {
    if (!dragging)
      return;

    dragging = false;
    ReleaseCapture();
  }

  void UpdateFonts(unsigned radius) noexcept;

  void DrawScale(Canvas &canvas, PixelPoint center, int radius,
                 double units_per_rev) const noexcept;
  void DrawCenterView(Canvas &canvas, PixelPoint center, int radius,
                      VarioDisplayCenterView view) const noexcept;
  void DrawWindArrows(Canvas &canvas, PixelPoint center, int radius,
                      bool double_arrow) const noexcept;
  void DrawThermalAssistant(Canvas &canvas, PixelPoint center, int radius,
                            bool spider) const noexcept;
  void DrawLineView(Canvas &canvas, PixelPoint pos, int radius,
                    VarioDisplayLineView view) const noexcept;
  void DrawInfo3(Canvas &canvas, PixelPoint center, int radius,
                 VarioDisplayInfo3View view) const noexcept;
  void DrawSatSymbol(Canvas &canvas, PixelPoint center,
                     int radius) const noexcept;
  void DrawNeedles(Canvas &canvas, PixelPoint center, int radius,
                   double degrees_per_unit) const noexcept;
  void DrawSpeedToFlyArc(Canvas &canvas, PixelPoint center,
                         int radius) const noexcept;

  void DrawMapOverlays(Canvas &canvas, const PixelRect &rc,
                       int status_height) const noexcept;
};
