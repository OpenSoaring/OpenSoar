// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

class Path;
class AllocatedPath;

/**
 * Manage the profile files.  The user can also mark another profile
 * for use, which takes effect on the next start.
 *
 * @param current_profile the profile in use, which cannot be
 * activated again; may be nullptr
 * @return true if another profile was activated
 */
bool
ProfileListDialog(Path current_profile);

/**
 * Let the user select a profile file.  Returns the absolute path of
 * the selected file or an empty string if the user has cancelled the
 * dialog.
 */
AllocatedPath
SelectProfileDialog(Path selected_path);
