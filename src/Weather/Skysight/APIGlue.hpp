// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Operation/Operation.hpp"

#include <string>


typedef void (*SkysightCallback) (
  const std::string details,
  const bool  success, 
  const std::string layer,
  const time_t time_index
);

enum class SkysightCallType {
  Login,
  Regions,
  Layers,
  LastUpdates,
  DataDetails,
  Data,
  Image,
  Tile
};

struct SkysightRequestArgs {
  const std::string url;
  const std::string path;
  const SkysightCallType calltype;
  const std::string region;
  const std::string layer;
  const uint64_t from;
  const uint64_t to;
  const SkysightCallback cb;
  SkysightRequestArgs(const std::string_view _url, const std::string_view _path,
		      const SkysightCallType _ct,
		      const std::string _region, const std::string _layer,
		      const uint64_t _from = 0, const uint64_t _to = 0,
		      const SkysightCallback _cb = nullptr):
    url(_url), path(_path), calltype(_ct),
    region(_region), layer(_layer), from(_from), to(_to), cb(_cb) {};
};
