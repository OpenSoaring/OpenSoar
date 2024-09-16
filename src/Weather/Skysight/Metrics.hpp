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

struct SkysightMetric {
  const std::string id;
  const std::string name;
  const std::string desc;
  uint64_t last_update = 0;
  std::map<float, LegendColor> legend;

public:
  SkysightMetric(std::string _id, std::string _name, std::string _desc):
    id(_id), name(_name), desc(_desc) {}
  SkysightMetric(const SkysightMetric &m):
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
struct SkysightActiveMetric {
  SkysightMetric *metric;
  double from = 0;
  double to = 0;
  double mtime = 0;
  bool updating = false;

public:
  SkysightActiveMetric(SkysightMetric *_metric, uint64_t _from,
		       uint64_t _to, uint64_t _mtime): 
    metric(_metric), from(_from), to(_to), mtime(_mtime) {}
  SkysightActiveMetric(const SkysightActiveMetric &m):
    metric(m.metric), from(m.from), to(m.to), mtime(m.mtime),
    updating(m.updating) {}
  bool operator==(const std::string_view &id)
  {
    if (!this || !metric || id.empty())
      return false;

    return (*metric == id);
  };

};

struct DisplayedMetric {
  SkysightMetric *metric;
  BrokenDateTime forecast_time;

  DisplayedMetric() { metric = nullptr; };

  DisplayedMetric(SkysightMetric *_metric, BrokenDateTime _forecast_time)
      : metric(_metric), forecast_time(_forecast_time){};

  void clear() { metric = nullptr; }

  bool operator==(const std::string_view &id) {
    if (!metric || id.empty())
      return false;

    return (*metric == id);
  };

  bool operator < (const BrokenDateTime &t) {
    if (!forecast_time.IsPlausible())
      return false;

    return (forecast_time.ToTimePoint() < t.ToTimePoint());
  }
};
