// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "system/Path.hpp"

#include <string>

/**
 * Manage the profile files.  The user can also mark another profile
 * for use, which takes effect on the next start; the dialog closes
 * right away then.
 *
 * @param current_profile the profile in use, which cannot be
 * activated again; may be nullptr
 * @param preselect the profile to put the cursor on; nullptr puts it
 * on the current profile
 * @return the profile that was activated, or nullptr
 */
AllocatedPath
ProfileListDialog(Path current_profile, Path preselect=nullptr);

/**
 * The profile list of the menu and of the system settings: when the
 * user activates another profile, a restart is offered, because only
 * a restart loads all of its settings; declined, the list comes back
 * on the activated profile.
 */
void
ShowProfileList() noexcept;

/**
 * The name of a profile as the dialogs show it: the file name without
 * the directory and without the suffix ".prf", which every profile
 * has and which only made the names longer ("August" instead of
 * "August.prf").
 */
std::string
ProfileDisplayName(Path path);

/**
 * Let the user select a profile file.  Returns the absolute path of
 * the selected file or an empty string if the user has cancelled the
 * dialog.
 */
AllocatedPath
SelectProfileDialog(Path selected_path);
