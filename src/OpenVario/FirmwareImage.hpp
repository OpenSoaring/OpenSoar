// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "system/Path.hpp"

#include <string>
#include <vector>

class OperationEnvironment;

/**
 * One firmware image the upgrade could flash: the name shown to the
 * user (file name without ".img.gz") and where it was found.
 */
struct FirmwareImage {
  std::string name;
  AllocatedPath path;

  /** the second row of the list: path, size and date */
  std::string detail;

  /** found in the download directory, so the Delete button may remove
      it; images on a USB stick stay where they are */
  bool deletable;
};

/**
 * The hardware an image is built for, read from its name: the parts
 * after the version, so "OV-3.2.20.1-CB2-CH57" gives "CB2-CH57" and
 * "OV-3.0.1-19-CB2-XXXX-testing" gives "CB2-XXXX".  Empty if the name
 * has no such part.
 */
[[gnu::pure]]
std::string
ImageDeviceType(const char *image_name) noexcept;

/**
 * The directory the Download button puts images into; on the device
 * $HOME/data/download on the data partition.
 */
AllocatedPath
GetFirmwareDownloadPath() noexcept;

/**
 * The directory $HOME/data/images on the data partition.  It holds
 * exactly one entry while an upgrade is pending: a hard link to the
 * image chosen for it.  ovmenu-ng.sh hands that image to
 * fw-upgrade.sh, and the recovery system looks for it there by name.
 */
AllocatedPath
GetFirmwareUpgradePath() noexcept;

/**
 * Collect the *.img.gz files from the download directory and from the
 * USB stick (openvario/images and openvario/download).  Each directory
 * is read on its own level only.  Sorted by name, without duplicates.
 */
std::vector<FirmwareImage>
FindFirmwareImages() noexcept;

/**
 * Make the given image the only entry of GetFirmwareUpgradePath().
 *
 * The entry is a hard link, so the image is not stored twice.  A hard
 * link cannot leave its file system, so an image from the USB stick is
 * first copied into the download directory (with progress, and
 * cancellable); it then stays there like a downloaded one.  Hard links
 * left from an earlier choice are removed.  A real file found there
 * (an image an older version put into data/images) is moved into the
 * download directory instead of being deleted.
 *
 * Throws on error.
 *
 * @return the path of the new entry, or nullptr if the user cancelled
 * the copy
 */
AllocatedPath
StageFirmwareImage(Path image, OperationEnvironment &env);
