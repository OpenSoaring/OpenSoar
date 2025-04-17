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
  time_t last_update = 0;
  std::map<float, LegendColor> legend;
  std::string time_name;

  double from = 0;
  double to = 0;
  double mtime = 0;
  bool updating = false;
#ifdef SKYSIGHT_LIVE
  bool live_layer = false;
  uint16_t zoom_min = 1;
  uint16_t zoom_max = 20;
#endif
  bool tile_layer = false;

  float alpha = 0.6;
  time_t forecast_time = 0;
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
};
