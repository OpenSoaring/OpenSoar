// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <memory>

class Widget;

/**
 * The OpenVario part of the "System Services" page: the image the
 * device runs, the program it starts after boot and the system
 * functions of the image.  Only to be shown on an OpenVario.
 */
std::unique_ptr<Widget>
CreateOpenVarioSystemWidget() noexcept;
