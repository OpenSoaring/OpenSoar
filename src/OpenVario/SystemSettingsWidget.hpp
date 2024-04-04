// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#ifdef IS_OPENVARIO

#include <memory>

class Widget;
class ContainerWindow;
struct DialogLook;

bool
ShowSystemSettingsWidget(ContainerWindow &parent,
                         const DialogLook &look) noexcept;

std::unique_ptr<Widget>
CreateSystemSettingsWidget() noexcept;

#endif