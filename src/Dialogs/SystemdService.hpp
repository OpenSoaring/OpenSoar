// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Linux/SystemdServiceList.hpp"

#include <optional>

/**
 * The installed service with the given id from
 * BuildSystemdServiceList() ("ssh", "sensord", "variod"), or nothing
 * if none of its units is installed.
 */
std::optional<SystemdService>
FindSystemdService(const char *id) noexcept;

/**
 * Is the service running?  False as well if systemd cannot be asked.
 */
bool
IsSystemdServiceActive(const SystemdService &service) noexcept;

/**
 * Switch a service on or off with a progress dialog.  On means a
 * restart (which starts a stopped service, and one that runs starts
 * again with its current configuration) and, once it runs, enabling
 * it for the next boot; off means stopping and disabling it.  If the
 * service does not stay running, the user is told why: a required
 * unit that is not running, the exit code of its program, and the
 * last lines of its journal.
 *
 * @return whether the service runs afterwards
 */
bool
SwitchSystemdService(const SystemdService &service, bool on) noexcept;
