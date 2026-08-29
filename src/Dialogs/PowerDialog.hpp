// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

/**
 * Ask the user how to leave the program: quit, restart, and - where
 * the target supports it - reboot or switch off the machine.  The
 * chosen action is remembered (PowerControl), and the shutdown
 * message appears right away, but the caller closes the program.
 *
 * @return true if the user chose an action, false on cancel
 */
bool
AskPowerAction() noexcept;

/**
 * Like AskPowerAction(), and closes the program when the user chose
 * an action.
 */
void
ShowPowerDialog() noexcept;

/**
 * Offer a restart after a setting that only takes effect on the next
 * start.  Does nothing but show a message where restarting is not
 * available (Android).
 *
 * @param message what was changed, shown above the question
 */
void
OfferRestart(const char *message) noexcept;
