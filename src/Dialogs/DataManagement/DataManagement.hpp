// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <memory>

class Widget;

void ShowDataManagementDialog();

/**
 * The data management functions as a page of the configuration dialog
 * (System > Data Management).
 */
std::unique_ptr<Widget>
CreateDataManagementPanel() noexcept;
