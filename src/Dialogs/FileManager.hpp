// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <memory>

class Widget;

void
ShowFileManager();

/**
 * The file manager with its buttons, as a page of the configuration
 * dialog (System > File Manager).
 */
std::unique_ptr<Widget>
CreateFileManagerPanel() noexcept;
