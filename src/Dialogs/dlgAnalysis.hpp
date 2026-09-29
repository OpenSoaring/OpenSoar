// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

struct Look;
namespace UI { class SingleWindow; }
class FullBlackboard;
class GlideComputer;
class Airspaces;
class RasterTerrain;

enum class AnalysisPage {
  BAROGRAPH,
  CLIMB,
  THERMAL_BAND,
  VARIO_HISTOGRAM,
  TASK_SPEED,
  WIND,
  POLAR,
  MACCREADY,
  TEMPTRACE,
  TASK,
  CONTEST,
  AIRSPACE,
  COUNT
};

/**
 * The page with the given name ("barograph", "climb", "thermal_band",
 * "vario_histogram", "task_speed", "wind", "polar", "maccready",
 * "temptrace", "task", "contest", "airspace"), for input events and
 * links.
 *
 * @return AnalysisPage::COUNT if the name is unknown or nullptr
 */
[[gnu::pure]]
AnalysisPage
ParseAnalysisPage(const char *name) noexcept;

void
dlgAnalysisShowModal(UI::SingleWindow &parent, const Look &look,
                     const FullBlackboard &blackboard,
                     GlideComputer &glide_computer,
                     const Airspaces *airspaces,
                     const RasterTerrain *terrain,
                     AnalysisPage page=AnalysisPage::COUNT);
