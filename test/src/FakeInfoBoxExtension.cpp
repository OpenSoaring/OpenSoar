// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

/*
 * Profile/InfoBoxConfig.cpp asks InfoBoxes/Content/Extension.cpp
 * whether a number read from a profile is a known InfoBox.  The real
 * Extension.cpp brings the contents of the OpenSoar InfoBoxes and
 * with them most of the program, so the profile tests link this
 * stand-in, which knows the two blocks but no hand-over to upstream.
 */

#include "InfoBoxes/Content/Extension.hpp"

bool
InfoBoxFactory::IsValid(Type type) noexcept
{
  return type < NUM_TYPES ||
    (type >= OPENSOAR_FIRST && type < OPENSOAR_END);
}

InfoBoxFactory::Type
InfoBoxFactory::Resolve(Type type) noexcept
{
  return type;
}
