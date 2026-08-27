// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#ifdef IS_OPENVARIO

#include <memory>

class Widget;
class ContainerWindow;
class DialogLook;

bool ShowDisplaySettingsWidget(ContainerWindow &parent,
                              const DialogLook &look) noexcept;

std::unique_ptr<Widget> 
CreateDisplaySettingsWidget() noexcept;
#endif