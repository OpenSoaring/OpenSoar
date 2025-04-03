// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#define SKYSIGHT_REQUEST_LOG
// #define SKYSIGHT_HTTP_LOG
#define SKYSIGHT_FILE_DEBUG

#include "APIGlue.hpp"
#include "thread/StandbyThread.hpp"

#include "system/Path.hpp"
#include "system/FileUtil.hpp"

// #include "Net/HTTP/Session.hpp"
#include "lib/curl/Request.hxx"
#include "lib/curl/Handler.hxx"

#include "io/FileLineReader.hpp"

#include "LogFile.hpp"
#include <string_view>

class SkysightRequestError {};

struct SkysightRequest {
public:
  enum class Status {Idle, Busy, Complete, Error};

  SkysightRequest(const SkysightRequestArgs _args): args(_args) {};
  bool Process();
  bool ProcessToString(std::string &response);

  SkysightCallType GetType();
  void SetCredentials(const std::string_view _key, const std::string_view _username = "",
                      const std::string_view _password = "");

  std::string_view GetCredentialKey();

  class FileHandler final: public CurlResponseHandler {
    FILE *file;
    size_t received = 0;
    Mutex mutex;
    Cond cond;

    std::exception_ptr error;

    bool done = false;

  public:
    FileHandler(FILE *_file): file(_file) {};
    // void DataReceived(const void *data, size_t length) override;
    // void ResponseReceived(int64_t content_length) override;
    void OnData(std::span<const std::byte> data) override;
    void OnHeaders(unsigned status, Curl::Headers &&headers) override;
    void OnEnd() override;
    void OnError(std::exception_ptr e) noexcept override;
    void Wait();
  };

  class BufferHandler final: public CurlResponseHandler {
    uint8_t *buffer;
    const size_t max_size;
    size_t received = 0;
    Mutex mutex;
    Cond cond;

    std::exception_ptr error;

    bool done = false;
    
  public:
    BufferHandler(void *_buffer, size_t _max_size):
      buffer((uint8_t *)_buffer), max_size(_max_size) {}
    size_t GetReceived() const;
    // void ResponseReceived(int64_t content_length) override;
    // void DataReceived(const void *data, size_t length) override;
    void OnData(std::span<const std::byte> data) override;
    void OnHeaders(unsigned status, Curl::Headers &&headers) override;
    void OnEnd() override;
    void OnError(std::exception_ptr e) noexcept override;
    void Wait();
  };

protected:
  const SkysightRequestArgs args;
  std::string_view key, username, password;
  bool RequestToFile();
  bool RequestToBuffer(std::string &response);
};

struct SkysightAsyncRequest final:
  public SkysightRequest, public StandbyThread {
public:
  SkysightAsyncRequest(const SkysightRequestArgs _args):
    SkysightRequest(_args), StandbyThread("SkysightAPIRequest"),
    status(Status::Idle) {};

  Status GetStatus();

  void Process();
  void Done();

  std::string GetMessage();

  SkysightCallType GetType();
  void SetCredentials(const std::string_view _key, const std::string_view _username = "",
    const std::string_view _password = "");

  void TriggerNullCallback(std::string &&ErrText);
  
protected:
  Status status;
  void Tick() noexcept override;
};
