// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

// Backwards-compatibility shim. The actual SteFly RemoteStick driver
// lives in src/Device/Driver/SteFly/ now (Internal.hpp, Misc.cpp,
// Parser.cpp, Settings.cpp, Register.cpp). Existing call sites that
// only need the DeviceRegister symbol (e.g. src/Device/Register.cpp)
// keep including this header.
//
// Code that wants to talk to the live device — for example the Manage
// dialog — should include "Device/Driver/SteFly/Internal.hpp" instead,
// which exposes the StickRemoteControl class.

#include "Device/Driver.hpp"

extern const struct DeviceRegister remote_stick_driver;
