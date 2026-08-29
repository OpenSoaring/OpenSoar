// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "UIActions.hpp"
#include "UIGlobals.hpp"
#include "Interface.hpp"
#include "Input/InputEvents.hpp"
#include "MainWindow.hpp"
#include "Language/Language.hpp"
#include "Dialogs/Message.hpp"
#include "Dialogs/PowerDialog.hpp"
#include "FLARM/Glue.hpp"
#include "Gauge/BigTrafficWidget.hpp"
#include "Gauge/BigThermalAssistantWidget.hpp"
#include "Look/Look.hpp"
#include "HorizonWidget.hpp"

static bool force_shutdown = false;
static UIActions::ExitAction exit_action = UIActions::ExitAction::NONE;

void
UIActions::SignalShutdown(bool force)
{
  force_shutdown = force;
  exit_action = force ? ExitAction::QUIT : ExitAction::NONE;
  CommonInterface::main_window->Close();
}

bool
UIActions::CheckShutdown() noexcept
{
  if (force_shutdown)
    return true;

  /* closing the window (system menu, Alt+F4, window manager) leads to
     the same dialog as the menu entry; it also offers reboot and power
     off where logind allows them (see PowerControl::IsAvailable()) */
  return AskPowerAction();
}

UIActions::ExitAction
UIActions::GetExitAction() noexcept
{
  return exit_action;
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
