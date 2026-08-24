// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ui/dim/Size.hpp"

PixelSize SystemWindowSize() noexcept;

PixelSize
SystemWindowSize() noexcept
{
  return PixelSize(100, 100);
}
