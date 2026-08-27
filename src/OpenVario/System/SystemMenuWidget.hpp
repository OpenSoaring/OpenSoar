// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#define OV_SETTINGS 0

#ifdef IS_OPENVARIO

#include <memory>

class Widget;
class ContainerWindow;
struct DialogLook;

bool 
ShowSystemMenuWidget(ContainerWindow &parent,
                              const DialogLook &look) noexcept;

std::unique_ptr<Widget> 
CreateSystemMenuWidget() noexcept;
#endif