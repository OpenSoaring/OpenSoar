// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <memory>

class Widget;

std::unique_ptr<Widget>
CreateVarioDisplayConfigPanel();

/**
 * The vario display page settings as a dialog of its own, for the
 * menu button and for a tap on the page.
 */
void
ShowVarioDisplayConfigDialog();
