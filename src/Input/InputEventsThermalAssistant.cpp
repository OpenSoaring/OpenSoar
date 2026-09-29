// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "InputEvents.hpp"
#include "Interface.hpp"
#include "MainWindow.hpp"
#include "PageActions.hpp"
#include "util/StringAPI.hxx"

/**
 * Show the thermal assistant page; "toggle" switches the small gauge
 * on the map, which appears while circling, off and on instead.
 */
void
InputEvents::eventThermalAssistant(const char *misc)
{
  if (misc != nullptr && StringIsEqual(misc, "toggle")) {
    CommonInterface::main_window->ToggleThermalAssistantGauge();
    return;
  }

  PageActions::ShowThermalAssistant();
}
