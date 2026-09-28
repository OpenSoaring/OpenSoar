// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "InputEvents.hpp"
#include "PageActions.hpp"
#include "Dialogs/Settings/Panels/PagesConfigPanel.hpp"
#include "util/StringAPI.hxx"

void
InputEvents::eventPage(const char *misc)
{
  if (StringIsEqual(misc, "restore"))
    PageActions::Restore();
  else if (StringIsEqual(misc, "settings"))
    ShowPageSettingsDialog();
}
