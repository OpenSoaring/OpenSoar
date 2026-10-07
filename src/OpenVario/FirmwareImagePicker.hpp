// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "system/Path.hpp"

/**
 * Let the user choose a firmware image from FindFirmwareImages(),
 * name in the first row and path, size and date in the second, with a
 * Download button where the repository offers images and a Delete
 * button that removes the highlighted image from the download
 * directory after a confirmation.  When the running image names the
 * hardware (ImageDeviceType()), the list and the download list show
 * only images for that hardware, and a button switches to all images
 * and back.
 *
 * @param current the image to preselect, or nullptr
 * @return the chosen (or just downloaded) image, or nullptr
 */
AllocatedPath
PickFirmwareImage(const char *caption, Path current) noexcept;
