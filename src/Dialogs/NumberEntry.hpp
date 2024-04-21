// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <tchar.h>

class Angle;

/** NumberEntryDialog for big unsigned numbers -> SIGNED with +/-! */
bool
NumberEntryDialog(const char *caption,
                  int &value, unsigned length);

/** NumberEntryDialog for big unsigned numbers -> UNSIGNED! */
bool
NumberEntryDialog(const char *caption,
                  unsigned &value, unsigned length);

bool AngleEntryDialog(const char *caption, Angle &value);
