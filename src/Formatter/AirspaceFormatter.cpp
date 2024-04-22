// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "AirspaceFormatter.hpp"
#include "Engine/Airspace/AbstractAirspace.hpp"
#include "util/Macros.hpp"

static const char *const airspace_class_names[] = {
  "Unknown",
  "Restricted",
  "Prohibited",
  "Danger Area",
  "Class A",
  "Class B",
  "Class C",
  "Class D",
  "No Gliders",
  "CTR",
  "Wave",
  "Task Area",
  "Class E",
  "Class F",
  "Transponder Mandatory Zone",
  "Class G",
  "Military Aerodrome Traffic Zone",
  "Radio Mandatory Zone",
};

static_assert(ARRAY_SIZE(airspace_class_names) ==
              (size_t)AirspaceClass::AIRSPACECLASSCOUNT,
              "number of airspace class names does not match number of "
              "airspace classes");

static const char *const airspace_class_short_names[] = {
  "?",
  "R",
  "P",
  "Q",
  "A",
  "B",
  "C",
  "D",
  "GP",
  "CTR",
  "W",
  "AAT",
  "E",
  "F",
  "TMZ",
  "G",
  "MATZ",
  "RMZ",
};

static_assert(ARRAY_SIZE(airspace_class_short_names) ==
              (size_t)AirspaceClass::AIRSPACECLASSCOUNT,
              "number of airspace class short names does not match number of "
              "airspace classes");

const char *
AirspaceFormatter::GetClass(AirspaceClass airspace_class)
{
  unsigned i = (unsigned)airspace_class;

  return i < ARRAY_SIZE(airspace_class_names) ?
         airspace_class_names[i] : NULL;
}

const char *
AirspaceFormatter::GetClassShort(AirspaceClass airspace_class)
{
  unsigned i = (unsigned)airspace_class;

  return i < ARRAY_SIZE(airspace_class_short_names) ?
         airspace_class_short_names[i] : NULL;
}

const char *
AirspaceFormatter::GetClass(const AbstractAirspace &airspace)
{
  return GetClass(airspace.GetClass());
}

const char *
AirspaceFormatter::GetClassShort(const AbstractAirspace &airspace)
{
  return GetClassShort(airspace.GetClass());
}

const char *
AirspaceFormatter::GetType(const AbstractAirspace &airspace)
{
  return airspace.GetType();
}
