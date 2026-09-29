// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <memory>

struct StaticEnumChoice;

class Widget;

std::unique_ptr<Widget>
CreateGaugesConfigPanel();

/** The positions of the thermal assistant gauge, shared with the map
    display dialog. */
extern const StaticEnumChoice thermal_assistant_position_list[];
