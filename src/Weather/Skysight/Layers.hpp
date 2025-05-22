// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <map>

  struct LegendColor {
    unsigned char Red;
    unsigned char Green;
    unsigned char Blue;
  };

  namespace SkySight {
    struct Layer {
    const std::string id;
    const std::string name;
    const std::string desc;
    std::string projection;
    std::string data_type;
    time_t last_update = 0;
    std::map<float, LegendColor> legend;
    std::string time_name;

    double from = 0;
    double to = 0;
    double mtime = 0;
    bool updating = false;
    bool tile_layer; // = false;
#ifdef SKYSIGHT_LIVE
    bool live_layer;  //  = false;
    uint16_t zoom_min = 1;
    uint16_t zoom_max;  // 20
#endif

    float alpha = 0.6;
    time_t forecast_time = 0;
  public:
    Layer(std::string _id, std::string _name, std::string _desc) :
      id(_id), name(_name), desc(_desc), tile_layer(false), live_layer(false),
      zoom_max(20) {}

    Layer(std::string _id, std::string _name, std::string _desc,
      bool _live_layer, bool _tile_layer, uint16_t _zoom_max = 20) :
      id(_id), name(_name), desc(_desc),
      tile_layer(_tile_layer), live_layer(_live_layer), zoom_max(_zoom_max) {}

    Layer(std::string_view _id) :
      id(_id), name(""), desc(""), tile_layer(false), live_layer(false),
      zoom_max(20) {}

    const Layer &operator =(const Layer &layer) { return layer; }

    bool operator==(std::string_view _id) {
      if (_id.empty()) return false;

      return (id == _id);
    };
  };
};