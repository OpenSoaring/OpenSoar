// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <memory>

class Widget;
class DataFieldEnum;

/**
 * Fill the choices of the display resolution.
 *
 * @param device_dpi the resolution of the device (system settings);
 * if it is set, the choice 0 means "the device setting" and says so,
 * otherwise it means "what the system reports"
 */
void
FillDpiChoices(DataFieldEnum &df, unsigned value,
               unsigned device_dpi=0) noexcept;

std::unique_ptr<Widget>
CreateDisplayConfigPanel();
