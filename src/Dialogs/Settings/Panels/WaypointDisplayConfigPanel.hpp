// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <memory>

struct StaticEnumChoice;

class Widget;

std::unique_ptr<Widget>
CreateWaypointDisplayConfigPanel();

/** The choices of the waypoint label visibility, shared with the map
    display dialog. */
extern const StaticEnumChoice wp_selection_list[];
