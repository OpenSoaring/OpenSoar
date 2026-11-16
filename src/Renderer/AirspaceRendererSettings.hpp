// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Airspace/AirspaceClass.hpp"
#include "ui/canvas/PortableColor.hpp"

#include <cstdint>

/** Airspace display modes */
enum class AirspaceDisplayMode: uint8_t
{
  ALLON = 0,
  CLIP,
  AUTO,
  ALLBELOW,
  INSIDE,
  ALLOFF
};

struct AirspaceClassRendererSettings
{
  /** Class-specific display flags */
  bool display;

  RGB8Color border_color;
  RGB8Color fill_color;

  unsigned border_width;

  /**
   * What portion of the airspace area should be filled?
   *
   * (Only used if the parent FillMode is not ALL)
   */
  enum class FillMode: uint8_t
  {
    /** fill all of the area */
    ALL,

    /** fill only a thick padding (like on ICAO maps) */
    PADDING,

    /** don't fill anything */
    NONE,
  } fill_mode;

  void SetDefaults();

  void SetColors(RGB8Color color) {
    border_color = fill_color = color;
  }
};

/**
 * Settings for airspace options
 */
struct AirspaceRendererSettings {
  bool enable;

  /** Airspaces are drawn with black border (otherwise in airspace color) */
  bool black_outline;

  /** Mode controlling how airspaces are filtered for display */
  AirspaceDisplayMode altitude_mode;

  /** Altitude (m) above which airspace is not drawn for clip mode */
  unsigned clip_altitude;

  /**
   * What portion of the airspace area should be filled?
   */
  enum class FillMode: uint8_t {
    /** the platform specific default is used */
    DEFAULT,

    /** fill all of the area */
    ALL,

    /** fill only a thick padding (like on ICAO maps) */
    PADDING,

    /** don't fill anything */
    NONE,

    /**
     * Draw the outlines one pixel wide (a little wider on the
     * OpenVario), whatever the border width of the class, and fill
     * only the airspaces that cause a warning or that the glider is
     * inside: the map stays as clear as possible without hiding what
     * matters now.
     */
    THIN_LINE,

    /**
     * The former separate mode "Thin lines, fill warnings", which
     * #THIN_LINE has become.  It stays only because the profile
     * stores the numeric value and profiles of OpenSoar 7.45 and 7.46
     * may contain it; Profile::Load() turns it into #THIN_LINE.
     */
    THIN_LINE_FILL_WARNINGS,
  } fill_mode;

  /**
   * Are all airspaces filled (according to the mode)?  False for
   * #THIN_LINE, which fills only some of them; see
   * FillsWarningsOnly().
   */
  constexpr bool HasFill() const noexcept {
    return fill_mode != FillMode::NONE && fill_mode != FillMode::THIN_LINE &&
      fill_mode != FillMode::THIN_LINE_FILL_WARNINGS;
  }

  /**
   * Are all outlines drawn thin, whatever their border width?
   */
  constexpr bool HasThinLines() const noexcept {
    return fill_mode == FillMode::THIN_LINE ||
      fill_mode == FillMode::THIN_LINE_FILL_WARNINGS;
  }

  /**
   * Are only the airspaces filled that cause a warning or that the
   * glider is inside?
   */
  constexpr bool FillsWarningsOnly() const noexcept {
    return HasThinLines();
  }

  /** What type of airspace labels to render */
  enum class LabelSelection : uint8_t {
    NONE,
    ALL,
  } label_selection;

  /** Show brief NOTAM text labels on the map when zoomed in enough */
  bool show_notam_labels;

  AirspaceClassRendererSettings classes[AIRSPACECLASSCOUNT];

  void SetDefaults();
};
