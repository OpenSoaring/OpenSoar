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
 * Only OpenVario builds (IS_OPENVARIO) have the page.  Without the
 * image, on a PC with Linux or Windows, it shows the same rows, and
 * their functions say that they are not available.
 */
std::unique_ptr<Widget>
CreateOpenVarioConfigPanel() noexcept;
