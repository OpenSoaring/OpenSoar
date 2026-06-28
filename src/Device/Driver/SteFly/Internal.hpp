// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

// Compatibility shim. The original single-header layout was split into
//   CommonDevice.hpp  ─ SteFlyDevice base class (info / reboot)
//   RemoteStick.hpp   ─ StickRemoteControl (joystick stick)
//   RotaryPanel.hpp   ─ RotaryPanelDevice  (encoder panel, skeleton)
//
// Keep this include as a one-stop for code that just wants the
// RemoteStick driver.

#include "RemoteStick.hpp"
