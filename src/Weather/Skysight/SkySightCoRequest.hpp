// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "co/InvokeTask.hxx"
#include "co/Task.hxx"
#include "event/Loop.hxx"
#include "event/DeferEvent.hxx"
#include "util/ReturnValue.hxx"
#include "lib/curl/Slist.hxx"

#include <string_view>

class Path;
class CurlGlobal;
class CurlSlist;
class ProgressListener;

class CoInstance {
  EventLoop event_loop;

  Co::InvokeTask invoke_task;

  DeferEvent defer_start{ event_loop, BIND_THIS_METHOD(OnDeferredStart) };

  std::exception_ptr error;

public:
  auto &GetEventLoop() noexcept {
    return event_loop;
  }

  void Run(Co::InvokeTask &&_task) {
    invoke_task = std::move(_task);
    defer_start.Schedule();
    event_loop.Run();
    if (error)
      std::rethrow_exception(error);
  }

  template<typename T>
  auto Run(Co::Task<T> &&task) {
    ReturnValue<T> result;
    Run(RunTask(std::move(task), result));
    return std::move(result).Get();
  }

private:
  template<typename T>
  Co::InvokeTask RunTask(Co::Task<T> task, ReturnValue<T> &result_r) {
    result_r.Set(co_await task);
  }

  void OnCompletion(std::exception_ptr _error) noexcept {
    error = std::move(_error);
    event_loop.Break();
  }

  void OnDeferredStart() noexcept {
    invoke_task.Start(BIND_THIS_METHOD(OnCompletion));
  }
};

class SkysightCoRequest {

private:
  std::string_view key; //  , username, password;
  //const 
  CurlSlist request_headers;
  // CurlSlist *request_headers = nullptr;
public:
  SkysightCoRequest(const std::string_view _key);

  void SetCredentialKey(const std::string_view _key);
//      const std::string_view _username,
//      const std::string_view _password);

#if 0
  // bool DownloadImageTest(const std::string_view url, const Path filename,
  bool DownloadImage(const std::string_view url, const Path filename,
    const std::string_view cred_key) noexcept;
#endif
  bool RequestCredentialKey(const std::string_view user, const std::string_view password) noexcept;

  bool DownloadImage(const std::string_view url, const Path filename) noexcept;

};