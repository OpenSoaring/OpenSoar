// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "UIActions.hpp"
#include "UIGlobals.hpp"
#include "Interface.hpp"
#include "Input/InputEvents.hpp"
#include "MainWindow.hpp"
#include "Language/Language.hpp"
#include "Dialogs/Message.hpp"
#include "FLARM/Glue.hpp"
#include "Gauge/BigTrafficWidget.hpp"
#include "Gauge/BigThermalAssistantWidget.hpp"
#include "Look/Look.hpp"
#include "HorizonWidget.hpp"

#ifdef _WIN32
// On Windows the default option should be forced!
static bool force_shutdown = true;
#else
static bool force_shutdown = false;
#endif

void
UIActions::SignalShutdown(bool force)
{
  force_shutdown = force;
  CommonInterface::main_window->Close();
}

bool
UIActions::CheckShutdown()
{
  if (force_shutdown)
    return true;

  switch (UI::TopWindow::GetExitValue()) {
#if defined(IS_OPENVARIO)
  case EXIT_REBOOT:
    return ShowMessageBox(_("Reboot System?"), "OpenSoar",
                          MB_YESNO | MB_ICONQUESTION) == IDYES;
  case EXIT_SHUTDOWN:
    return ShowMessageBox(_("Shutdown System?"), "OpenSoar",
                          MB_YESNO | MB_ICONQUESTION) == IDYES;
  case EXIT_NEWSTART:
    return ShowMessageBox(_("Quit and Restart OpenSoar?"), "OpenSoar",
                          MB_YESNO | MB_ICONQUESTION) == IDYES;
#endif
  case EXIT_RESTART:
    return ShowMessageBox(_("Short Internal Restart?"), "OpenSoar",
                          MB_YESNO | MB_ICONQUESTION) == IDYES;

  case EXIT_SYSTEM:
  default:
    return ShowMessageBox(_("Quit program?"), "OpenSoar",
                          MB_YESNO | MB_ICONQUESTION) == IDYES;
  }
}

void
UIActions::ShowTrafficRadar()
{
  if (InputEvents::IsFlavour("Traffic"))
    return;

  LoadFlarmDatabases();

  CommonInterface::main_window->SetWidget(new TrafficWidget());
  InputEvents::SetFlavour("Traffic");
}

void
UIActions::ShowThermalAssistant()
{
  if (InputEvents::IsFlavour("TA"))
    return;

  auto ta_widget =
    new BigThermalAssistantWidget(CommonInterface::GetLiveBlackboard(),
                                  UIGlobals::GetLook().thermal_assistant_dialog);
  CommonInterface::main_window->SetWidget(ta_widget);
  InputEvents::SetFlavour("TA");
}

void
UIActions::ShowHorizon()
{
  if (InputEvents::IsFlavour("Horizon"))
    return;

  auto widget = new HorizonWidget();
  CommonInterface::main_window->SetWidget(widget);
  InputEvents::SetFlavour("Horizon");
}
