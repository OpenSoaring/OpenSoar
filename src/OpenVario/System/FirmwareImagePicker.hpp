// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "system/Path.hpp"

#include <string>
#include <vector>

class DataField;

/**
 * One firmware image the upgrade could flash: the name shown to the
 * user (file name without ".img.gz") and where it was found.
 */
struct FirmwareImage {
  std::string name;
  AllocatedPath path;
};

/**
 * Collect the *.img.gz files from every place an OpenVario looks for
 * them: the images directory next to the data directory, the USB stick
 * the device mounts, any removable drive that carries opensoar/images
 * (this is how a PC stands in for the stick), and the data directory
 * itself, where downloaded images land.  Sorted by name, without
 * duplicates.
 */
std::vector<FirmwareImage>
FindFirmwareImages() noexcept;

/**
 * Let the user choose a firmware image from FindFirmwareImages(),
 * name in the first row and the full path in the second, with a
 * Download button where the repository offers images.  A choice is
 * stored in the data field through ForceModify(), so the field's
 * listener sees it like any other change.
 *
 * Signature of WndProperty::EditCallback.
 *
 * @return true if a file was chosen
 */
bool
PickFirmwareImage(const char *caption, DataField &df,
                  const char *help_text) noexcept;
