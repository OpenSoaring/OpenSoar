// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

namespace UI {
class Display;
class EventQueue;
}

/**
 * Calibrate the sensor board with "sensorcal", offering to initialise
 * a board that has never been initialised.  The sensor daemon is
 * stopped meanwhile, because it holds the sensors open.  Errors are
 * shown to the user.
 */
void
CalibrateSensors() noexcept;

/**
 * Is there a touch screen input device the calibration could work
 * with?  On the OpenVario this is the touch controller of the SoC,
 * which exists even when no touch panel is connected, so "true" does
 * not prove that a panel will answer.
 */
[[gnu::pure]]
bool
HasTouchScreenDevice() noexcept;

/**
 * Run the touch screen calibration script of the OpenVario image.
 * It draws on the console itself, so this process gives up the
 * display and stops reading input until the script has finished.
 *
 * Since nothing can interrupt the script meanwhile, it is stopped
 * when the touch screen stays silent for a while (no panel connected,
 * or nobody touching it), so the device cannot hang in the
 * calibration.  The user is told why it stopped.
 */
void
CalibrateTouch(UI::Display &display, UI::EventQueue &event_queue) noexcept;
