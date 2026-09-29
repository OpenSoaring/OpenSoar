// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <memory>

struct StaticEnumChoice;

class Widget;

std::unique_ptr<Widget>
CreateSymbolsConfigPanel();

/** The choices of the trail length, shared with the map display dialog. */
extern const StaticEnumChoice trail_length_list[];
