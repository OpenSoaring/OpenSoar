// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <memory>

class Widget;

std::unique_ptr<Widget>
CreatePagesConfigPanel();

/**
 * Edit the configured layout of the current page in a dialog of its
 * own, without going through the configuration menu.
 */
void
ShowPageSettingsDialog() noexcept;
