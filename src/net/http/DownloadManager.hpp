// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Features.hpp"

#include <cstdint>
#include <exception>
#include <string_view>
#include <boost/json.hpp>

class Path;

namespace Net {

  struct CurlData;

class DownloadListener {
public:
  enum DownloadType {
    FILE,
    JSON,
    BUFFER
  };
  /**
   * This is called by the #DownloadManager when a new download was
   * added, and when DownloadManager::Enumerate() is called.
   *
   * @param size the total size of the file when the download has
   * been finished; -1 if unknown
   * @param position the number of bytes already downloaded; -1 if
   * the download is queued, but has not been started yet
   */
//  virtual void OnDownloadAdded(Path path_relative,
  virtual void OnDownloadAdded(std::string_view name, // const DownloadType type,
                               size_t size = 0, size_t position = 0) noexcept = 0;
//
//  virtual void OnDownloadComplete(std::string_view name, 
//                                  const DownloadType type) noexcept = 0;
//  virtual void OnDownloadAdded(const std::string_view name,
//                               int64_t size, int64_t position) noexcept = 0;

  virtual void OnDownloadComplete(const std::string_view name) noexcept = 0;

  /**
   * The download has failed or was canceled.
   *
   * @param error error details; may be empty (e.g. if this was due to
   * cancellation)
   */
  virtual void OnDownloadError(const std::string_view name,
                               std::exception_ptr error) noexcept = 0;
};

} // namespace Net

namespace Net::DownloadManager {

#ifdef HAVE_DOWNLOAD_MANAGER
bool Initialise() noexcept;
void BeginDeinitialise() noexcept;
void Deinitialise() noexcept;

[[gnu::const]]
bool IsAvailable() noexcept;

void AddListener(DownloadListener &listener) noexcept;
void RemoveListener(DownloadListener &listener) noexcept;

/**
 * Enumerate the download queue, and invoke
 * DownloadListener::OnDownloadAdded() for each one.
 */
void Enumerate(DownloadListener &listener) noexcept;

void Enqueue(const std::string_view uri, const Path path_relative) noexcept;
// void Enqueue(const std::string_view uri, const std::string_view name) noexcept;
void Enqueue(const std::string_view uri, const Path path, Net::CurlData *data) noexcept;
  void Enqueue(const std::string_view uri, const std::string_view name,
  boost::json::value &json) noexcept;

void Enqueue(const std::string_view uri, const std::string_view name, 
  Net::CurlData *data) noexcept;

/**
 * Cancel the download.  The download may however be already
 * finished before this function attempts the cancellation.
 */
void Cancel(const std::string_view name) noexcept;
#else

static constexpr bool IsAvailable() noexcept {
  return false;
}
#endif

} // namespace Net::DownloadManager
