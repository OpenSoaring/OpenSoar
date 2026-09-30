// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "MenuData.hpp"

void
Menu::Clear() noexcept
{
  for (auto &i : items)
    i.Clear();
}

void
Menu::Add(const char *label, unsigned location, unsigned event_id,
          Placement placement) noexcept
{
  if (location >= items.size())
    return;

  MenuItem &item = items[location];

  item.label = label;
  item.event = event_id;

  /* a placement out of range is ignored, the item then stays at its
     location */
  item.portrait = placement.portrait < items.size() ? placement.portrait : 0;
  item.landscape = placement.landscape < items.size()
    ? placement.landscape : 0;

  /* no screen shows more than a few columns; a larger number is a
     typing error and counts as the default */
  auto columns = [](unsigned n) noexcept -> uint8_t {
    return n <= MenuPlacement::MAX_PLACE_COLUMNS ? n : 0;
  };
  item.portrait_columns = columns(placement.portrait_columns);
  item.landscape_columns = columns(placement.landscape_columns);

  item.center = placement.center;
}

bool
Menu::IsEmpty() const noexcept
{
  for (const auto &i : items)
    if (i.IsDefined())
      return false;

  return true;
}

int
Menu::FindByEvent(unsigned event) const noexcept
{
  for (std::size_t i = 0; i < items.size(); ++i)
    if (items[i].event == event)
      return i;

  return -1;
}
