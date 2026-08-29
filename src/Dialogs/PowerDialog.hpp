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
 * start.  Only shows a message where restarting is not available.
 *
 * @param message what was changed, shown above the question
 * @return true if the program is shutting down for a restart, false
 * if the user declined or restarting is not available
 */
bool
OfferRestart(const char *message) noexcept;

/**
 * Tell the user and restart right away, no question asked - for a
 * change the running program cannot carry on with, such as a
 * restored backup.  Where restarting is not available, the program
 * quits instead.
 *
 * @param message what happened, shown above the notice
 */
void
RestartNow(const char *message) noexcept;
