// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <memory>

class Widget;

/**
 * The page System > OpenVario: the program the image starts after
 * boot (with the versions installed), the image the device runs with
 * the firmware upgrade behind it, the services sensord and variod as
 * switches, the sensor calibration and the system backup and restore.
 * On another Linux it shows the services only, if they are installed.
 */
std::unique_ptr<Widget>
CreateOpenVarioConfigPanel() noexcept;
