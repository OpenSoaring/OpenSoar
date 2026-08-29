// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "system/Path.hpp"

/**
 * Manage the profile files.  The user can also mark another profile
 * for use, which takes effect on the next start; the dialog closes
 * right away then.
 *
 * @param current_profile the profile in use, which cannot be
 * activated again; may be nullptr
 * @param preselect the profile to put the cursor on; may be nullptr
 * @return the profile that was activated, or nullptr
 */
AllocatedPath
ProfileListDialog(Path current_profile, Path preselect=nullptr);

/**
 * Let the user select a profile file.  Returns the absolute path of
 * the selected file or an empty string if the user has cancelled the
 * dialog.
 */
AllocatedPath
SelectProfileDialog(Path selected_path);
