// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "TerminalLook.hpp"
#include "FontDescription.hpp"
#include "Screen/Layout.hpp"

void
TerminalLook::Initialise()
{
#ifdef IS_OPENVARIO_CB2
  font.Load(FontDescription(Layout::FontScale(6), false, false, true));
#else
  font.Load(FontDescription(Layout::FontScale(8), false, false, true));
#endif
}
