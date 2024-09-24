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

  double from = 0;
  double to = 0;
  double mtime = 0;
  bool updating = false;

  BrokenDateTime forecast_time = BrokenDateTime(0, 0, 0);
public:
  SkysightLayer(std::string _id, std::string _name, std::string _desc):
    id(_id), name(_name), desc(_desc) {}

  SkysightLayer(std::string_view _id):
    id(_id), name(""), desc("") {}

#if 1
  SkysightLayer(const SkysightLayer &layer):
    id(layer.id), name(layer.name), desc(layer.desc), last_update(layer.last_update),
    legend(layer.legend),
    from(layer.from), to(layer.to), mtime(layer.mtime),
    updating(layer.updating) {}

  const SkysightLayer &operator =(const SkysightLayer &layer) { return layer; }
#endif

  bool operator==(std::string_view _id) {
    if (_id.empty()) return false;

    return (id == _id);
  };

  bool operator < (const BrokenDateTime &t) {
    if (!forecast_time.IsPlausible())
      return false;

    return (forecast_time.ToTimePoint() < t.ToTimePoint());
  }

};


#if 0
/*
 * Skysight chart which is overlaid
 */
#if 1
#define SkysightActiveLayer SkysightLayer
#define DisplayedLayer SkysightLayer
#else 
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
#endif
#endif
