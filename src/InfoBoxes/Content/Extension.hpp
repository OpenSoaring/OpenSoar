// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Type.hpp"
#include "Group.hpp"

#include <memory>

class InfoBoxContent;

/**
 * OpenSoar's additions to the InfoBox factory, kept apart from the
 * upstream tables so that an XCSoar update does not touch them:
 *
 * - the InfoBoxes of the OpenSoar block (Type::e_OPENSOAR_FIRST ...),
 *   with their names, captions, descriptions, groups and contents;
 *
 * - the hand-over to upstream: when XCSoar adopts one of the
 *   OpenSoar boxes under a number of its own, the OpenSoar entry
 *   names that number, a profile carrying the old number is
 *   rewritten on load, and the entry disappears from the picker
 *   without moving any other number.
 */
namespace InfoBoxFactory {

/**
 * Is this a type the factory knows: an upstream one, or one of the
 * OpenSoar block?  Placeholders and superseded boxes count as valid
 * (they have a name and can be displayed); see IsSelectable().
 */
[[gnu::const]]
bool
IsValid(Type type) noexcept;

/**
 * Shall the picker offer this type?  False for placeholders (a number
 * reserved by the old OpenSoar that has no box in this version) and
 * for boxes XCSoar has adopted since (Resolve() maps those).
 */
[[gnu::const]]
bool
IsSelectable(Type type) noexcept;

/**
 * The type to use for a type read from a profile: an OpenSoar box
 * that XCSoar has adopted comes back as the upstream type, everything
 * else unchanged.  Invalid types come back unchanged as well - the
 * caller decides what to do with them.
 */
[[gnu::const]]
Type
Resolve(Type type) noexcept;

/**
 * The name/caption/description/group/content of a type of the OpenSoar
 * block.  Called by the upstream factory functions for types beyond
 * NUM_TYPES; not meant to be called directly.
 */
namespace Extension {

[[gnu::const]] const char *GetName(Type type) noexcept;
[[gnu::const]] const char *GetCaption(Type type) noexcept;
[[gnu::const]] const char *GetDescription(Type type) noexcept;
[[gnu::const]] Group GetGroup(Type type) noexcept;
std::unique_ptr<InfoBoxContent> Create(Type type) noexcept;

} // namespace Extension

} // namespace InfoBoxFactory
