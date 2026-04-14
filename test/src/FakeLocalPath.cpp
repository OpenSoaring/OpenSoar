// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "LocalPath.hpp"
#include "system/Path.hpp"
#include "system/FileUtil.hpp"

#include <string_view>

AllocatedPath
LocalPath([[maybe_unused]] Path file) noexcept
{
  return nullptr;
}

AllocatedPath
LocalPath([[maybe_unused]] const std::string_view file) noexcept
{
  return nullptr;
}

void
VisitDataFiles([[maybe_unused]] const char *filter,
  [[maybe_unused]] File::Visitor &visitor,
  [[maybe_unused]] bool recursive)
{
}

AllocatedPath
ExpandLocalPath([[maybe_unused]] Path src) noexcept
{
  return nullptr;
}

AllocatedPath
ContractLocalPath([[maybe_unused]] Path src) noexcept
{
  return nullptr;
}

AllocatedPath
GetCachePath([[maybe_unused]] std::string_view path_name) noexcept
{
  return nullptr;
}

Path
GetCachePath() noexcept
{
  return nullptr;
}
