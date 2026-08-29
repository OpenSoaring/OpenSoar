// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

// IWYU pragma: begin_exports
#include "Profile/Keys.hpp"
#include "Profile/ProfileMap.hpp"
// IWYU pragma: end_exports

#include <string_view>
#include <vector>

class Path;
class AllocatedPath;

namespace Profile {

/**
 * Returns the absolute path of the current profile file.
 */
[[gnu::pure]]
Path
GetPath() noexcept;

/**
 * Loads the profile files
 */
void
Load() noexcept;

/**
 * Loads the given profile file
 */
void
LoadFile(Path path) noexcept;

/**
 * Has Load() been called?  Before that, there is nothing worth
 * saving: the map is empty, and saving would wipe the file.
 */
[[gnu::pure]]
bool
IsLoaded() noexcept;

/**
 * Saves the profile into the profile files, if anything was
 * modified since the last save.  A profile marked for the next start
 * (see MarkForNextStart()) is touched afterwards, so that it stays
 * the most recent one.
 *
 * Errors will be caught and logged.
 */
void
Save() noexcept;

/**
 * Mark another profile as the one to use from the next start on:
 * the running profile is saved first, then the other file gets the
 * newest timestamp - and keeps it, whatever is saved later (see
 * Save()).
 *
 * @return false if the file could not be touched
 */
bool
MarkForNextStart(Path path) noexcept;

/**
 * The profile marked with MarkForNextStart(), or nullptr.
 */
[[gnu::pure]]
Path
GetNextStartPath() noexcept;

/**
 * Saves the profile into the given profile file
 */
void
SaveFile(Path path);

/**
 * Sets the profile files to load when calling Load()
 * @param override nullptr or file to load when calling Load()
 */
void
SetFiles(Path override_path) noexcept;

/**
 * Reads a configured path from the profile, and expands it with
 * ExpandLocalPath().
 *
 * @param value a buffer which can store at least MAX_PATH
 * characters
 */
[[gnu::pure]]
AllocatedPath
GetPath(std::string_view key) noexcept;

std::vector<AllocatedPath> GetMultiplePaths(std::string_view key,
                                            const char *patterns);

void
SetPath(std::string_view key, Path value) noexcept;

[[gnu::pure]]
bool
GetPathIsEqual(std::string_view key, Path value) noexcept;

} // namespace Profile
