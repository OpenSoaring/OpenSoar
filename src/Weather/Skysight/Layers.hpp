// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "time/BrokenDateTime.hpp"
#include <map>

struct LegendColor {
  unsigned char Red;
  unsigned char Green;
  unsigned char Blue;
};

struct SkysightLayer {
  const std::string id;
  const std::string name;
  const std::string desc;
  uint64_t last_update = 0;
  std::map<float, LegendColor> legend;

public:
  SkysightLayer(std::string _id, std::string _name, std::string _desc):
    id(_id), name(_name), desc(_desc) {}
  SkysightLayer(const SkysightLayer &m):
    id(m.id), name(m.name), desc(m.desc), last_update(m.last_update),
    legend(m.legend) {}
  bool operator==(std::string_view _id) {
    if (_id.empty()) return false;

    return (id == _id);
  };
};

/*
 * Skysight chart which is overlaid
 */
struct SkysightActiveLayer {
  SkysightLayer *layer;
  double from = 0;
  double to = 0;
  double mtime = 0;
  bool updating = false;

public:
  SkysightActiveLayer(SkysightLayer *_layer, uint64_t _from,
		       uint64_t _to, uint64_t _mtime): 
    layer(_layer), from(_from), to(_to), mtime(_mtime) {}
  SkysightActiveLayer(const SkysightActiveLayer &m):
    layer(m.layer), from(m.from), to(m.to), mtime(m.mtime),
    updating(m.updating) {}
  bool operator==(const std::string_view &id)
  {
    if (!this || !layer || id.empty())
      return false;

    return (*layer == id);
  };

};

struct DisplayedLayer {
  SkysightLayer *layer;
  BrokenDateTime forecast_time;

  DisplayedLayer() { layer = nullptr; };

  DisplayedLayer(SkysightLayer *_layer, BrokenDateTime _forecast_time)
      : layer(_layer), forecast_time(_forecast_time){};

  void clear() { layer = nullptr; }

  bool operator==(const std::string_view &id) {
    if (!layer || id.empty())
      return false;

    return (*layer == id);
  };

  bool operator < (const BrokenDateTime &t) {
    if (!forecast_time.IsPlausible())
      return false;

    return (forecast_time.ToTimePoint() < t.ToTimePoint());
  }
};
