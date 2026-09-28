// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <string.h>

#include <array>
#include <cstdint>

/**
 * The flight phases in which a quick menu item is offered, as a bit
 * mask.  An item without any bit is offered in all of them.
 */
namespace MenuPhase {

static constexpr uint8_t ALL = 0;
static constexpr uint8_t GROUND = 1 << 0;
static constexpr uint8_t FLIGHT = 1 << 1;
static constexpr uint8_t AFTER = 1 << 2;

/**
 * Is an item with the phase mask @p phases offered in the view
 * @p view (one phase, or #ALL for all items)?
 */
constexpr bool
IsOffered(uint8_t phases, uint8_t view) noexcept
{
  return view == ALL || phases == ALL || (phases & view) != 0;
}

} // namespace MenuPhase

/**
 * Data of an item in the mode menu.
 */
class MenuItem {
public:
  const char *label;
  unsigned event;

  /**
   * The flight phases in which the quick menu offers this item, see
   * #MenuPhase.
   */
  uint8_t phases;

  void Clear() noexcept {
    label = nullptr;
    event = 0;
    phases = MenuPhase::ALL;
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

  void Add(const char *label, unsigned location, unsigned event_id,
           uint8_t phases=MenuPhase::ALL) noexcept;

  /**
   * Does any item belong to some flight phases only?  Only then the
   * quick menu offers to switch between them.
   */
  [[gnu::pure]]
  bool HasPhases() const noexcept;

  [[gnu::pure]]
  int FindByEvent(unsigned event) const noexcept;
};
