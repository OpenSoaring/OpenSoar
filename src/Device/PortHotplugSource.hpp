// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "PortHotplug.hpp"

/**
 * Create this platform's source of port reports.
 *
 * @return nullptr if this platform has none, reports through
 * PostPortHotplug() (Android), or the source could not be set up
 */
std::unique_ptr<PortHotplug::Source>
CreatePortHotplugSource(PortHotplug &hotplug,
                        EventLoop &event_loop) noexcept;
