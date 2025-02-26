// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Request.hpp"
#include "CDFDecoder.hpp"
#include "Layers.hpp"
#include "ui/event/PeriodicTimer.hpp"
#include <vector>

class SkysightAPIQueue final {
  std::vector<std::unique_ptr<SkysightAsyncRequest>> request_queue;
  std::vector<std::unique_ptr<CDFDecoder>> decode_queue;
  bool is_busy = false;
  bool is_clearing = false;
  std::string key;
  uint64_t key_expiry_time = 0;
  std::string_view email;
  std::string_view password;

  void Process();
  UI::PeriodicTimer timer{[this]{ Process(); }};

public:
  SkysightAPIQueue() {};
  ~SkysightAPIQueue();

  void SetCredentials(const std::string_view _email, const std::string_view _pass);
  void SetKey(const std::string _key, const uint64_t _key_expiry_time);
  bool IsLoggedIn();
  void AddRequest(std::unique_ptr<SkysightAsyncRequest> request,
		  bool append_end = true);
  void AddDecodeJob(std::unique_ptr<CDFDecoder> &&job);
  void Clear(const std::string msg);
  bool IsEmpty() {
    return (request_queue.empty() && decode_queue.empty());
  }
  bool IsLastJob() {
    return ((request_queue.size() + decode_queue.size()) <= 1) ;
  }
  void DoClearingQueue();
};
