// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Request.hpp"
#ifdef SKYSIGHT_FORECAST 
# include "CDFDecoder.hpp"
#endif  // SKYSIGHT_FORECAST 
#include "Layers.hpp"
#include "ui/event/PeriodicTimer.hpp"
#include <vector>
#include <memory>

class SkysightAPIQueue final {
  std::vector<std::unique_ptr<SkysightAsyncRequest>> request_queue;
#ifdef SKYSIGHT_FORECAST 
  std::vector<std::unique_ptr<CDFDecoder>> decode_queue;
#endif  // SKYSIGHT_FORECAST 
  bool is_busy = false;
  bool is_clearing = false;
  std::string key;
  time_t key_expiry_time = 0;
  std::string_view email;
  std::string_view password;

  void Process();
  UI::PeriodicTimer timer{[this]{ Process(); }};

public:
  SkysightAPIQueue() {};
  ~SkysightAPIQueue();

  void SetCredentials(const std::string_view _email, const std::string_view _pass);
  void SetKey(const std::string _key, const uint64_t _key_expiry_time);
#if 0 // brauche ich erst einmal nicht!
  std::string_view GetKey(uint64_t &expiry_time) {
    expiry_time = key_expiry_time;
    return key;
  }
#endif
  bool IsLoggedIn();
  void AddRequest(std::unique_ptr<SkysightAsyncRequest> request,
		  bool append_end = true);
#ifdef SKYSIGHT_FORECAST 
  void AddDecodeJob(std::unique_ptr<CDFDecoder> &&job);
#endif  // SKYSIGHT_FORECAST 
  void Clear(const std::string msg);
#ifdef SKYSIGHT_FORECAST 
  bool IsEmpty() {
    return (request_queue.empty() && decode_queue.empty());
  }
  bool IsLastJob() {
    return ((request_queue.size() + decode_queue.size()) <= 1) ;
  }
#else  // SKYSIGHT_FORECAST 
  bool IsEmpty() {
    return (request_queue.empty());
  }
  bool IsLastJob() {
    return (request_queue.size() <= 1) ;
  }
#endif // SKYSIGHT_FORECAST 

  void DoClearingQueue();
};
