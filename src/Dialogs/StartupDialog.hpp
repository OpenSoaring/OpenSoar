// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

class Path;
/**
 * @return true on success, false if the user has pressed the "Quit"
 * button
 */
// bool
// dlgStartupShowModal() noexcept;
bool
dlgStartupShowModal(Path profile) noexcept;
