// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "APIGlue.hpp"
#include "thread/StandbyThread.hpp"
#include "Layers.hpp"
#include "system/Path.hpp"

#include <map>

class CDFDecoder final : public StandbyThread {
public:
  enum class Status {Idle, Busy, Complete, Error};

private:
  const std::string path;
  const AllocatedPath output_path;
  const std::string data_varname;
  const uint64_t time_index;
  const std::map<float, LegendColor> legend;
  SkysightCallback callback;
  Status status;
  void Tick() noexcept override;
  bool Decode();
  void MakeCallback(bool result);
  bool DecodeError();
  bool DecodeSuccess();

public:
  enum class Result {Available, Requested, Error};

  CDFDecoder(const std::string_view _path, const std::string &&_output, const std::string &&_varname,
             const uint64_t _time_index, const std::map<float, LegendColor> _legend, SkysightCallback _callback) : 
             StandbyThread("CDFDecoder"), path(_path), output_path(AllocatedPath(_output.c_str())), 
             data_varname(_varname), time_index(_time_index), legend(_legend), callback(_callback), 
             status(Status::Idle) {};
  ~CDFDecoder() {};

  void DecodeAsync();
  void Done();
  Status GetStatus();
};
