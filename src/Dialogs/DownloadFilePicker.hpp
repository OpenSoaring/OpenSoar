// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Repository/FileType.hpp"

class AllocatedPath;

/**
 * An optional restriction of the download list to the files whose
 * name carries a token - the OpenVario uses it to list only the
 * firmware images for the hardware it runs on.  The dialog shows a
 * check box "Only <label>" for it, on by default.
 */
struct DownloadNameFilter {
  /** what the check box says, e.g. "CB2-CH70" */
  const char *label;

  /** the token a file name has to carry as a whole part (see
      StringHasPart()), e.g. "CB2-CH70" matches
      "OV-3.2.20.1-CB2-CH70.img.gz" */
  const char *token;

  /** the initial state of the check box */
  bool enabled;
};

/**
 * @param name_filter if not nullptr, the list gets a check box that
 * hides the files whose name does not carry the token; the caller
 * keeps the struct alive while the dialog runs, and finds the check
 * box's final state in its "enabled" member afterwards
 */
AllocatedPath
DownloadFilePicker(FileType file_type,
                   DownloadNameFilter *name_filter = nullptr);
