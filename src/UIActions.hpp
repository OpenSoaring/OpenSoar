// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

/**
 * This namespace provides access to several user interface actions.
 * These are usually triggered by the user, for example by
 * (configurable) InputEvents or by hard-coded button handlers.  This
 * API is meant to be lean, without too many header dependencies.
 */
namespace UIActions {
  enum class ExitAction {
    NONE,
    QUIT,
    REBOOT,
    POWER_OFF,
  };

  void SignalShutdown(bool force);

  bool CheckShutdown() noexcept;

  /**
   * Forget a shutdown that was signalled before, for the next run of
   * an internal restart.
   */
  void ResetShutdown() noexcept;

  [[gnu::pure]]
  ExitAction GetExitAction() noexcept;

  /**
   * Switch to the traffic radar page.
   */
  void ShowTrafficRadar();

  void ShowThermalAssistant();

  void ShowHorizon();

  void ShowVarioDisplay();
};
