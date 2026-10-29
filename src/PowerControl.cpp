// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "PowerControl.hpp"
#include "ExitValues.hpp"

#ifdef KOBO
#include "Kobo/System.hpp"
#endif

static PowerAction pending_action = PowerAction::NONE;

bool
PowerControl::IsAvailable(PowerAction action) noexcept
{
  switch (action) {
  case PowerAction::NONE:
    return false;

  case PowerAction::QUIT:
    return true;

  case PowerAction::RESTART:
    /* everywhere: the program runs Main() once more within the same
       process (see TakeRestart()), so it needs nobody's help - not
       the base menu of the OpenVario, not the Android activity */
    return true;

  case PowerAction::REBOOT:
  case PowerAction::SHUTDOWN:
    /* only where the program can reach the machine: the Kobo does it
       itself, the OpenVario base menu does it for us */
#if defined(KOBO) || defined(IS_OPENVARIO)
    return true;
#else
    return false;
#endif
  }

  return false;
}

void
PowerControl::Set(PowerAction action) noexcept
{
  pending_action = action;
}

bool
PowerControl::TakeRestart() noexcept
{
  if (pending_action != PowerAction::RESTART)
    return false;

  pending_action = PowerAction::NONE;
  return true;
}

int
PowerControl::Perform(int exit_code) noexcept
{
  const auto action = pending_action;
  pending_action = PowerAction::NONE;

  switch (action) {
  case PowerAction::NONE:
  case PowerAction::QUIT:
    break;

  case PowerAction::RESTART:
    /* taken by TakeRestart() before; only reached if a restart was
       chosen where nobody runs Main() again, and then it quits */
    break;

  case PowerAction::REBOOT:
#ifdef KOBO
    KoboReboot();
#elif defined(IS_OPENVARIO)
    return EXIT_REBOOT;
#endif
    break;

  case PowerAction::SHUTDOWN:
#ifdef KOBO
    KoboPowerOff();
#elif defined(IS_OPENVARIO)
    return EXIT_SHUTDOWN;
#endif
    break;
  }

  return exit_code;
}
