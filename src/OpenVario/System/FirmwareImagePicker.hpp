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
 * them: the download directory (GetUserDownloadsPath(): the device's
 * data/download, or the OpenVario and XCSoar subdirectories of the
 * user's Downloads folder on a PC or a phone), the USB stick the device
 * mounts at /usb/usbstick/openvario/images, and any removable drive
 * that carries openvario/images, which is how a PC stands in for the
 * stick.  Each directory is read on its own level only.  Sorted by
 * name, without duplicates.
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
