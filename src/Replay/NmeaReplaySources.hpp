// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/**
 * The data sources of an NMEA log in the order of their rank, and
 * whether each of them takes part in the replay.
 *
 * In flight, each device has its own data set, and the merge gives
 * the first device of the list the say for every value it provides;
 * the devices below only fill in what is missing.  A log written with
 * a head on each line ("hhmmss.mmm - A Driver: $...") tells the
 * replay which device a sentence came from, so that the replay can
 * keep the data of each device apart and merge them in the same way.
 * The user may switch sources off and change their order, to see
 * what a different arrangement of the devices would have made of
 * the same flight.
 *
 * A source is named by the label of its head ("A Generic"); a log
 * written before the slot letter was added has only the driver name
 * ("Generic"), and lines without any head make up the source with the
 * empty label.
 *
 * This list belongs to the #Replay, not to the #NmeaReplay instance:
 * jumping back in the file starts a new #NmeaReplay, and the choice of
 * the user must survive that.
 */
class NmeaReplaySources {
public:
  struct Source {
    std::string label;
    bool enabled = true;

    /**
     * Does this source send positions ($xxRMC)?  Only such a source
     * can pace the replay.
     */
    bool has_position = false;
  };

private:
  std::vector<Source> list;

public:
  /**
   * Does the label start with the slot letter of the device ("A ")?
   */
  static constexpr bool HasSlotLetter(std::string_view label) noexcept {
    return label.size() >= 3 && label[0] >= 'A' && label[0] <= 'Z' &&
      label[1] == ' ';
  }

  /**
   * The name of the driver that wrote the sentences of this source;
   * empty for lines without a head.
   */
  static constexpr std::string_view
  GetDriverName(std::string_view label) noexcept {
    return HasSlotLetter(label) ? label.substr(2) : label;
  }

  /**
   * Split a line of an NMEA log into the label of its head and the
   * sentence.  A line that starts with the sentence has the empty
   * label.  Returns nullptr for a line that holds no sentence at all.
   */
  static const char *SplitLine(const char *buffer,
                               std::string_view &label) noexcept {
    if (buffer[0] == '$') {
      label = {};
      return buffer;
    }

    const char *sentence = std::strstr(buffer, ": $");
    if (sentence == nullptr)
      return nullptr;

    /* the head is "hhmmss.mmm - Label"; take what follows the dash,
       or all of it if there is no dash */
    const std::string_view head{buffer, std::size_t(sentence - buffer)};
    const auto dash = head.find(" - ");
    label = dash == head.npos ? head : head.substr(dash + 3);
    return sentence + 2; // ':' and ' '
  }

  void Clear() noexcept {
    list.clear();
  }

  bool empty() const noexcept {
    return list.empty();
  }

  std::size_t size() const noexcept {
    return list.size();
  }

  const Source &operator[](std::size_t i) const noexcept {
    return list[i];
  }

  auto begin() const noexcept {
    return list.begin();
  }

  auto end() const noexcept {
    return list.end();
  }

  /**
   * The rank of the source with this label, or -1 if it is not known
   * yet.
   */
  int Find(std::string_view label) const noexcept {
    for (std::size_t i = 0; i < list.size(); ++i)
      if (list[i].label == label)
        return int(i);
    return -1;
  }

  /**
   * Add a new source at the rank it had in flight: sources with a slot
   * letter are ordered by that letter, because the letter is their
   * position in the device list; sources without one (an older log)
   * follow in the order in which they first appear.  Returns the rank.
   */
  unsigned Add(std::string_view label) {
    std::size_t i = list.size();
    if (HasSlotLetter(label)) {
      for (i = 0; i < list.size(); ++i)
        if (!HasSlotLetter(list[i].label) || list[i].label[0] > label[0])
          break;
    }

    list.insert(list.begin() + i, Source{std::string{label}});
    return unsigned(i);
  }

  unsigned FindOrAdd(std::string_view label) {
    const int i = Find(label);
    return i >= 0 ? unsigned(i) : Add(label);
  }

  void SetHasPosition(std::size_t i) noexcept {
    list[i].has_position = true;
  }

  /**
   * The source whose positions pace the replay: the enabled source of
   * the highest rank that sends positions.  Returns -1 if there is
   * none.
   */
  int GetPacingSource() const noexcept {
    for (std::size_t i = 0; i < list.size(); ++i)
      if (list[i].enabled && list[i].has_position)
        return int(i);
    return -1;
  }

  void Toggle(std::size_t i) noexcept {
    list[i].enabled = !list[i].enabled;
  }

  /**
   * Swap the source with the one above it, giving it the higher rank.
   */
  void MoveUp(std::size_t i) noexcept {
    if (i > 0 && i < list.size())
      std::swap(list[i - 1], list[i]);
  }
};
