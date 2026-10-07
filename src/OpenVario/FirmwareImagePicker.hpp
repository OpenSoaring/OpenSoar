// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "system/Path.hpp"

/**
 * Let the user choose a firmware image from FindFirmwareImages(),
 * name in the first row and path, size and date in the second.  When
 * the running image names the hardware (ImageDeviceType()), the list
 * shows only images for that hardware, and a button switches to all
 * images and back.
 *
 * @return the chosen image, or nullptr
 */
AllocatedPath
PickFirmwareImage(const char *caption) noexcept;
