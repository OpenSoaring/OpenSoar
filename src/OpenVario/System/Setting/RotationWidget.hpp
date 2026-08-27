// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <memory>

// class Widget;
class ContainerWindow;
class DialogLook;

bool 
ShowRotationSettingsWidget(ContainerWindow &parent,
                               const DialogLook &look) noexcept;


// std::unique_ptr<Widget> CreateSettingRotationWidget() noexcept;
