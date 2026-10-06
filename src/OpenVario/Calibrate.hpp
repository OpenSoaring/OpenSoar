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
 * Run the touch screen calibration script of the OpenVario image.
 * It draws on the console itself, so this process gives up the
 * display and stops reading input until the script has finished.
 */
void
CalibrateTouch(UI::Display &display, UI::EventQueue &event_queue) noexcept;
