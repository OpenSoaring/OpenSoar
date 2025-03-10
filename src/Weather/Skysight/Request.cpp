// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Request.hpp"
#include "APIGlue.hpp"
#include "LogFile.hpp"
#include "SkysightAPI.hpp"
#include "Version.hpp"
#include "io/FileLineReader.hpp"
#include "lib/curl/Handler.hxx"
#include "lib/curl/Request.hxx"
#include "lib/curl/Slist.hxx"
#include "net/http/Init.hpp"
#include "system/FileUtil.hpp"
#include "system/Path.hpp"
#include "thread/StandbyThread.hpp"
#include "util/StaticString.hxx"

#if defined(SKYSIGHT_FILE_DEBUG)
# include "time/DateTime.hpp"
# include <chrono>
# include <filesystem>
# include <fstream>
# include <regex>
# ifdef USE_STD_FORMAT
#   include <format>
# else
#   include <fmt/format.h>
# endif
#endif

void
SkysightRequest::FileHandler::OnData(std::span<const std::byte> data)
{
  size_t written = fwrite(data.data(), 1, data.size(), file);
  if (written != data.size())
    throw SkysightRequestError();

  received += data.size();
}

void
SkysightRequest::FileHandler::OnHeaders([[maybe_unused]] unsigned status,
    [[maybe_unused]]  Curl::Headers &&headers) {
#ifdef SKYSIGHT_HTTP_LOG
  LogFormat("FileHandler::OnHeaders %d", status);
#endif
}

void
SkysightRequest::FileHandler::OnEnd() {
  const std::lock_guard<Mutex> lock(mutex);
  done = true;
  cond.notify_one();
}

void
SkysightRequest::FileHandler::OnError(std::exception_ptr e) noexcept {
  LogError(e);
  const std::lock_guard<Mutex> lock(mutex);
  error = std::move(e);
  done = true;
  cond.notify_one();
}

void
SkysightRequest::FileHandler::Wait() {
  std::unique_lock<Mutex> lock(mutex);
  cond.wait(lock, [this]{ return done; });

  if (error)
    std::rethrow_exception(error);
}

size_t
SkysightRequest::BufferHandler::GetReceived() const
{
  return received;
}

void
SkysightRequest::BufferHandler::OnData(std::span<const std::byte> data)
{
  memcpy(buffer + received, data.data(), data.size());
  received += data.size();
}

void
SkysightRequest::BufferHandler::OnHeaders([[maybe_unused]] unsigned status,
    [[maybe_unused]]  Curl::Headers &&headers) {
#ifdef SKYSIGHT_HTTP_LOG
  LogFormat("BufferHandler::OnHeaders %d", status);
#endif
}

void
SkysightRequest::BufferHandler::OnEnd() {
  const std::lock_guard<Mutex> lock(mutex);
  done = true;
  cond.notify_one();
}

void
SkysightRequest::BufferHandler::OnError(std::exception_ptr e) noexcept {
  LogError(e);
  const std::lock_guard<Mutex> lock(mutex);
  error = std::move(e);
  done = true;
  cond.notify_one();
}

void
SkysightRequest::BufferHandler::Wait() {
  std::unique_lock<Mutex> lock(mutex);
  cond.wait(lock, [this] { return done; });

  if (error)
    std::rethrow_exception(error);
}

SkysightRequest::Status
SkysightAsyncRequest::GetStatus()
{
  std::lock_guard<Mutex> lock(mutex);
  Status s = status;
  return s;
}

SkysightCallType
SkysightRequest::GetType()
{
  return args.calltype;
}

void
SkysightRequest::SetCredentials(const char *_key, const char *_username,
                                const char *_password)
{
  key = _key;
  if(_username != nullptr)
    username = _username;
  if(_password)
    password = _password;
}

SkysightCallType
SkysightAsyncRequest::GetType()
{
  std::lock_guard<Mutex> lock(mutex);
  SkysightCallType ct = args.calltype;
  return ct;
}

void
SkysightAsyncRequest::SetCredentials(const char *_key,
				     const char *_username,
				     const char *_password)
{
  std::lock_guard<Mutex> lock(mutex);
  SkysightRequest::SetCredentials(_key, _username, _password);
}

void
SkysightAsyncRequest::TriggerNullCallback(std::string &&ErrText)
{
  if(args.cb)
    args.cb(ErrText.c_str(), false, args.layer.c_str(), args.from);
}

bool
SkysightRequest::Process()
{
  return RequestToFile();
}

std::string
SkysightAsyncRequest::GetMessage()
{
  std::lock_guard<Mutex> lock(mutex);
  std::string msg = std::string("Downloading ") + args.layer;
  return msg;
}

bool
SkysightRequest::ProcessToString(std::string &response)
{
  return RequestToBuffer(response);
}

void
SkysightAsyncRequest::Process()
{
  std::lock_guard<Mutex> lock(mutex);
  if (IsBusy()) return;
  Trigger();
}

void
SkysightAsyncRequest::Tick() noexcept
{
  status = Status::Busy;

  mutex.unlock();

  bool result;
  std::string resultStr;

  if (!args.path.empty()) {
    result = RequestToFile();
    resultStr = args.path.c_str();
  } else {
    result = RequestToBuffer(resultStr);
  }

  if (result) {
    SkysightAPI::ParseResponse(resultStr.c_str(), result, args);
  } else {
    SkysightAPI::ParseResponse("Could not fetch data from Skysight server.",
                               result, args);
  }

  mutex.lock();
  status = (!result) ? Status::Error : Status::Complete;
}

void
SkysightAsyncRequest::Done()
{
  StandbyThread::LockStop();
}

bool
SkysightRequest::RequestToFile()
{
#ifdef SKYSIGHT_REQUEST_LOG
  LogFormat("Connecting to %s for %s with key:%s user-agent:%s",
            args.url.c_str(), args.path.c_str(), key.c_str(),
            OpenSoar_ProductToken);
#endif
  Path final_path(args.path.c_str());
  AllocatedPath temp_path = final_path.WithSuffix(".dltemp");

  if (!File::Delete(temp_path) && File::ExistsAny(temp_path))
    return  false;

  FILE *file = fopen(temp_path.c_str(), "wb");
  if (file == nullptr)
    return false;

  bool success = true;
  FileHandler handler(file);
  CurlRequest request(*Net::curl, args.url.c_str(), handler);
  CurlSlist request_headers;

  request_headers.AppendFormat("%s: %s", "X-API-Key", key.c_str());
  request_headers.AppendFormat("%s: %s", "User-Agent", OpenSoar_ProductToken);

  std::string pBody;
  switch (GetType()) {
    case SkysightCallType::Login:
      if (username.length() && password.length()) {
        request_headers.AppendFormat("%s: %s", "Content-Type", "application/json");
        StaticString<1024> creds;
        creds.Format("{\"username\":\"%s\",\"password\":\"%s\"}", username.c_str(),
          password.c_str());
        pBody = creds.c_str();
        request.GetEasy().SetRequestBody(pBody.c_str(), pBody.length());
        request.GetEasy().SetFailOnError(false);
      }
      break;
    case SkysightCallType::Data:
    case SkysightCallType::Image:
      request_headers.AppendFormat("%s: %s", "Content-Type", "gzip");
      break;
    default:
      request_headers.AppendFormat("%s: %s", "Content-Type", "application/json");
      break;
  }

  request.GetEasy().SetRequestHeaders(request_headers.Get());
  request.GetEasy().SetVerifyPeer(false);

  try {
    request.StartIndirect();
    handler.Wait();
  } catch (const std::exception &exc) {
    success = false;
  }

  success &= fclose(file) == 0;

  if (!success) File::Delete(temp_path);

  file = NULL;

  if (success) {
    if (!File::Delete(final_path) && File::ExistsAny(final_path)) {
      File::Delete(temp_path);
      return false;
    }
    std::rename(temp_path.c_str(), args.path.c_str());
#if defined(SKYSIGHT_FILE_DEBUG)
    std::stringstream filename;

    std::string name = temp_path.GetBase().c_str();
    name = std::regex_replace(name, std::regex("\""), "");
    name = std::regex_replace(name, std::regex("[.]dltemp"), "");

    filename << "skysight/" << name << '-' << 
      DateTime::str_now("%Y%m%d_%H%M%S") << ".tmp";
    AllocatedPath debug_path = LocalPath(filename.str());
    if (std::filesystem::exists(debug_path.c_str())) {
      LogFormat("file %s exists!", debug_path.c_str());
    } else {
      std::filesystem::copy_file(args.path.c_str(), debug_path.c_str());
    }
#endif
  }

  return success;
}

bool
SkysightRequest::RequestToBuffer(std::string &response)
{
  bool success = false;
  if (args.url.empty())
  {
#ifdef SKYSIGHT_REQUEST_LOG
    LogFormat("Connecting to %s for %s with key:%s user-agent:%s",
              args.url.c_str(), args.path.c_str(), key.c_str(),
              OpenSoar_ProductToken);
#endif

    char buffer[10240];
    BufferHandler handler(buffer, sizeof(buffer));
    CurlRequest request(*Net::curl, args.url.c_str(), handler);
    CurlSlist request_headers;

    request_headers.AppendFormat("%s: %s", "X-API-Key", key.c_str());
    request_headers.AppendFormat("%s: %s", "User-Agent",
                                 OpenSoar_ProductToken);

    std::string pBody;
    if (username.length() && password.length())
    {
      request_headers.AppendFormat("%s: %s", "Content-Type",
                                   "application/json");
      StaticString<1024> creds;
      creds.Format("{\"username\":\"%s\",\"password\":\"%s\"}",
                   username.c_str(), password.c_str());
      pBody = creds.c_str();
      request.GetEasy().SetRequestBody(pBody.c_str(), pBody.length());
      request.GetEasy().SetFailOnError(false);
    }

    request.GetEasy().SetRequestHeaders(request_headers.Get());
    request.GetEasy().SetVerifyPeer(false);

    try
    {
      request.StartIndirect();
      handler.Wait();
      success = true;
    }
    catch (const std::exception &exc)
    {
      success = false;
    }

    response = std::string(buffer,
                           buffer + handler.GetReceived() / sizeof(buffer[0]));
#if defined(SKYSIGHT_FILE_DEBUG)
    std::string filename("skysight/");

    auto name = LocalPath(args.path).GetBase();
    filename += DateTime::str_now() + ' ';
    filename += name.str() + ".txt";

    AllocatedPath debug_path = LocalPath(filename);

    std::fstream fs;
    fs.open(debug_path.c_str(), std::fstream::out | std::fstream::app);

    fs << "url:      " << args.url << std::endl;
    fs << "path:     " << args.path << std::endl;
    fs << "key:      " << key << std::endl;
    fs << "token:    " << OpenSoar_ProductToken << std::endl;
    fs << "calldate: " << DateTime::now() << " - "
      << std::endl;
    if (args.url.find("from_time") != std::string::npos)
    {
      fs << "from:     "
        << DateTime::str_now()
        << std::endl;
    }
    fs << "==================================== " << std::endl;
    fs << response;

    fs.close();
#endif
  }

  return success;
}
