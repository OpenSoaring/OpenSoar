// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <string_view>

class SoundUtil {
public:
  static bool Play(const std::string_view resource_name);
};
