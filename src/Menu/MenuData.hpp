// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <string.h>

#include <array>
#include <cstdint>

/**
 * Data of an item in the mode menu.
 */
class MenuItem {
public:
  const char *label;
  unsigned event;

  /**
   * Where the quick menu places this item in portrait and in
   * landscape: a location counted row by row in the given number of
   * columns (0 = the default of the list).  A location of 0 means the
   * location of the item.  The location stays what identifies the
   * item, so a later input file can still replace it; these only move
   * it on the screen, which has more columns in landscape.
   */
  uint8_t portrait, landscape;
  uint8_t portrait_columns, landscape_columns;

  /**
   * Does the quick menu start with the focus on this item (and on
   * its page)?
   */
  bool center;

  void Clear() noexcept {
    label = nullptr;
    event = 0;
    portrait = landscape = 0;
    portrait_columns = landscape_columns = 0;
    center = false;
  }

  constexpr bool IsDefined() const noexcept {
    return event > 0;
  }

  /**
   * Does this item have a dynamic label?  It may need updates more
   * often, because the variables that the label depends on may change
   * at any time.
   */
  [[gnu::pure]]
  bool IsDynamic() const noexcept {
    return label != nullptr && strstr(label, "$(") != nullptr;
  }
};

/**
 * The placement of an item in the quick menu, see
 * MenuItem::portrait, MenuItem::landscape and MenuItem::center.  It is
 * not nested in class Menu, because a nested class with default
 * member initializers cannot be the default argument of a method of
 * the enclosing class yet.
 */
struct MenuPlacement {
  /** the most columns a place may be counted in */
  static constexpr unsigned MAX_PLACE_COLUMNS = 16;

  unsigned portrait = 0, landscape = 0;
  unsigned portrait_columns = 0, landscape_columns = 0;
  bool center = false;
};

/**
 * A container for MenuItem objects.
 */
class Menu {
public:
  static constexpr std::size_t MAX_ITEMS = 64;

protected:
  std::array<MenuItem, MAX_ITEMS> items;

public:
  void Clear() noexcept;

  const MenuItem &operator[](unsigned i) const noexcept {
    return items[i];
  }

  using Placement = MenuPlacement;

  void Add(const char *label, unsigned location, unsigned event_id,
           Placement placement={}) noexcept;

  /**
   * Does the menu have no item at all?
   */
  [[gnu::pure]]
  bool IsEmpty() const noexcept;

  [[gnu::pure]]
  int FindByEvent(unsigned event) const noexcept;
};
