// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project
#pragma once

#ifdef IS_OPENVARIO

#include <memory>

class Widget;
class ContainerWindow;
struct DialogLook;

bool 
ShowFileMenuWidget(ContainerWindow &parent,
                              const DialogLook &look) noexcept;

std::unique_ptr<Widget> 
CreateFileMenuWidget() noexcept;

#endif