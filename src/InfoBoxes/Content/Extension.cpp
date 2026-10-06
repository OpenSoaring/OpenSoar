// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "InfoBoxes/Content/Extension.hpp"
#include "InfoBoxes/Content/Base.hpp"
#include "InfoBoxes/Content/OpenSoar.hpp"
#include "Language/Language.hpp"
#include "util/Macros.hpp"

#include <cassert>

using namespace InfoBoxFactory;

/**
 * An InfoBox whose content is a plain update function (the OpenSoar
 * block has no panels).
 */
class InfoBoxContentUpdate final : public InfoBoxContent {
  void (*const update)(InfoBoxData &data) noexcept;

public:
  explicit InfoBoxContentUpdate(void (*_update)(InfoBoxData &data) noexcept) noexcept
    :update(_update) {}

  void Update(InfoBoxData &data) noexcept override {
    update(data);
  }
};

/**
 * One entry of the OpenSoar block.  A placeholder (no update
 * function) keeps a number occupied that the old OpenSoar used for a
 * box this version does not carry.
 */
struct ExtensionMetaData {
  Group group;

  const char *name;
  const char *caption;
  const char *description;

  void (*update)(InfoBoxData &data) noexcept;

  /**
   * The upstream type XCSoar has adopted this box under, or NUM_TYPES
   * while it is still OpenSoar's alone.  Once set, Resolve() maps the
   * OpenSoar number to it and the picker hides this entry.
   */
  Type upstream;

  constexpr bool IsPlaceholder() const noexcept {
    return update == nullptr;
  }

  constexpr bool IsSuperseded() const noexcept {
    return upstream != NUM_TYPES;
  }
};

/* WARNING: the order is the order of the enum (Type.hpp) and of the
   old OpenSoar profiles.  Never insert, delete or rearrange - a box
   that goes away becomes a placeholder. */
static constexpr ExtensionMetaData opensoar_meta_data[] = {
  // e_DriftAngle (500): test box of the old OpenSoar, not carried
  { Group::OTHER, nullptr, nullptr, nullptr, nullptr, NUM_TYPES },

  // e_InstantaneousWindSpeed
  { Group::WIND,
    N_("Wind - live speed"),
    N_("Live Wind"),
    N_("Speed of the instantaneous wind reported by an external sensor "
       "(Anemoi, ...), unfiltered; the bearing in the comment."),
    UpdateInfoBoxInstantaneousWindSpeed, NUM_TYPES },

  // e_InstantaneousWindBearing
  { Group::WIND,
    N_("Wind - live bearing"),
    N_("Live Wind"),
    N_("Bearing of the instantaneous wind reported by an external "
       "sensor (Anemoi, ...), unfiltered; the speed in the comment."),
    UpdateInfoBoxInstantaneousWindBearing, NUM_TYPES },

  // e_InternalWind
  { Group::WIND,
    N_("Wind - internal estimate"),
    N_("Int Wind"),
    N_("The wind XCSoar estimates itself from circling and from the "
       "EKF, whatever the effective wind is set to (external sensor, "
       "manual)."),
    UpdateInfoBoxInternalWind, NUM_TYPES },

  // e_InternalZigZagWind
  { Group::WIND,
    N_("Wind - zig-zag estimate"),
    N_("ZigZag Wind"),
    N_("The wind of the zig-zag (EKF) estimator, shown while it is the "
       "source of the effective wind."),
    UpdateInfoBoxInternalZigZagWind, NUM_TYPES },

  // e_PageNo
  { Group::SYSTEM,
    N_("Page number"),
    N_("Page"),
    N_("Number of the page on display and the number of pages, "
       "with the kind of the page in the comment."),
    UpdateInfoBoxPageNo, NUM_TYPES },

  // e_STFSwitch
  { Group::SETTING,
    N_("STF switch"),
    N_("STF"),
    N_("State of the speed-to-fly / vario switch as reported by the "
       "connected vario."),
    UpdateInfoBoxSTFSwitch, NUM_TYPES },

  // e_BugsSetting
  { Group::SETTING,
    N_("Bugs setting"),
    N_("Bugs"),
    N_("The bugs setting: 100 % is a clean wing."),
    UpdateInfoBoxBugsSetting, NUM_TYPES },

  // e_TrueHeading
  { Group::SPEED,
    N_("True heading"),
    N_("Heading"),
    N_("True heading from the attitude sensor of a connected device."),
    UpdateInfoBoxTrueHeading, NUM_TYPES },

  // e_WaterBallast
  { Group::SETTING,
    N_("Water ballast"),
    N_("Ballast"),
    N_("Water ballast in litres, with the share of the maximum in the "
       "comment."),
    UpdateInfoBoxWaterBallast, NUM_TYPES },

  // e_Mouse, e_Coordinates, e_MouseDistance: developer boxes of the
  // old OpenSoar, not carried
  { Group::OTHER, nullptr, nullptr, nullptr, nullptr, NUM_TYPES },
  { Group::OTHER, nullptr, nullptr, nullptr, nullptr, NUM_TYPES },
  { Group::OTHER, nullptr, nullptr, nullptr, nullptr, NUM_TYPES },
};

static_assert(ARRAY_SIZE(opensoar_meta_data) == NUM_OPENSOAR_TYPES,
              "Wrong OpenSoar InfoBox table size");

static constexpr bool
IsOpenSoarType(Type type) noexcept
{
  return type >= OPENSOAR_FIRST && type < OPENSOAR_END;
}

static constexpr const ExtensionMetaData &
GetExtension(Type type) noexcept
{
  assert(IsOpenSoarType(type));
  return opensoar_meta_data[type - OPENSOAR_FIRST];
}

/*
 * validity and hand-over
 */

bool
InfoBoxFactory::IsValid(Type type) noexcept
{
  return type < NUM_TYPES || IsOpenSoarType(type);
}

bool
InfoBoxFactory::IsSelectable(Type type) noexcept
{
  if (type < NUM_TYPES)
    return true;

  if (!IsOpenSoarType(type))
    return false;

  const auto &m = GetExtension(type);
  return !m.IsPlaceholder() && !m.IsSuperseded();
}

Type
InfoBoxFactory::Resolve(Type type) noexcept
{
  if (!IsOpenSoarType(type))
    return type;

  const auto &m = GetExtension(type);
  return m.IsSuperseded() ? m.upstream : type;
}

/*
 * the factory functions for the OpenSoar block
 */

const char *
InfoBoxFactory::Extension::GetName(Type type) noexcept
{
  if (!IsOpenSoarType(type))
    return nullptr;

  const auto &m = GetExtension(type);
  /* a placeholder has a name in the picker's "invalid" sense: none */
  return m.name;
}

const char *
InfoBoxFactory::Extension::GetCaption(Type type) noexcept
{
  if (!IsOpenSoarType(type))
    return nullptr;

  return GetExtension(type).caption;
}

const char *
InfoBoxFactory::Extension::GetDescription(Type type) noexcept
{
  if (!IsOpenSoarType(type))
    return nullptr;

  return GetExtension(type).description;
}

Group
InfoBoxFactory::Extension::GetGroup(Type type) noexcept
{
  if (!IsOpenSoarType(type))
    return Group::OTHER;

  return GetExtension(type).group;
}

std::unique_ptr<InfoBoxContent>
InfoBoxFactory::Extension::Create(Type type) noexcept
{
  if (!IsOpenSoarType(type))
    return nullptr;

  const auto &m = GetExtension(type);
  if (m.IsPlaceholder())
    return nullptr;

  return std::make_unique<InfoBoxContentUpdate>(m.update);
}
