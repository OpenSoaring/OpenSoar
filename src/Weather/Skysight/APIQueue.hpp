// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#ifdef SKYSIGHT_FORECAST 
# include "CDFDecoder.hpp"
#endif
#include "Layers.hpp"
#include "ui/event/PeriodicTimer.hpp"
#include <vector>
#include <memory>

#ifdef SKYSIGHT_FORECAST 
class SkysightAPIQueue final {
  std::vector<std::unique_ptr<CDFDecoder>> decode_queue;
  bool is_busy = false;
  bool is_clearing = false;
  std::string key;
  [[maybe_unused]] time_t key_expiry_time = 0;
  std::string_view email;
  std::string_view password;
  [[maybe_unused]] SkysightRequest *co_request;

  void Process();
  UI::PeriodicTimer timer{[this]{ Process(); }};

public:
  SkysightAPIQueue() {};
  ~SkysightAPIQueue();

  void AddDecodeJob(std::unique_ptr<CDFDecoder> &&job);
  void Clear(const std::string msg);
  bool IsEmpty() {
    return decode_queue.empty();
  }
  bool IsLastJob() {
    return (decode_queue.size() <= 1) ;
  }

  void DoClearingQueue();
};
#endif
