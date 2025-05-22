// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "DownloadManager.hpp"
#include "system/Path.hpp"
#include "LogFile.hpp"
#include "Version.hpp"
#include "system/FileUtil.hpp"
#include <string>

// #ifdef ANDROID
#ifdef NO_ANDROID

#include "Android/DownloadManager.hpp"
#include "Android/Main.hpp"

static AndroidDownloadManager *download_manager;

bool
Net::DownloadManager::Initialise() noexcept
{
  assert(download_manager == nullptr);

  if (!AndroidDownloadManager::Initialise(Java::GetEnv()))
    return false;

  try {
    download_manager = new AndroidDownloadManager(Java::GetEnv(), *context);
    return true;
  } catch (...) {
    LogError(std::current_exception(),
             "Failed to initialise the DownloadManager");
    return false;
  }
}

void
Net::DownloadManager::BeginDeinitialise() noexcept
{
}

void
Net::DownloadManager::Deinitialise() noexcept
{
  delete download_manager;
  download_manager = nullptr;
  AndroidDownloadManager::Deinitialise(Java::GetEnv());
}

bool
Net::DownloadManager::IsAvailable() noexcept
{
  return download_manager != nullptr;
}

void
Net::DownloadManager::AddListener(DownloadListener &listener) noexcept
{
  assert(download_manager != nullptr);

  download_manager->AddListener(listener);
}

void
Net::DownloadManager::RemoveListener(DownloadListener &listener) noexcept
{
  assert(download_manager != nullptr);

  download_manager->RemoveListener(listener);
}

void
Net::DownloadManager::Enumerate(DownloadListener &listener) noexcept
{
  assert(download_manager != nullptr);

  download_manager->Enumerate(Java::GetEnv(), listener);
}

void
Net::DownloadManager::Enqueue(const std::string_view uri, const std::string_view name,
  boost::json::value &json) noexcept
  // Net::DownloadManager::Enqueue(const std::string_view uri, const Path path_relative) noexcept
{
  assert(download_manager != nullptr);

  download_manager->Enqueue(Java::GetEnv(), uri, name, json);
}

void
Net::DownloadManager::Enqueue(const std::string_view uri, const Path path_relative) noexcept
{
  assert(download_manager != nullptr);

  download_manager->Enqueue(Java::GetEnv(), uri, path_relative);
    // Net::DownloadListener::FILE);
}

void
// Net::DownloadManager::Enqueue(const std::string_view uri, boost::json::value json) noexcept
Net::DownloadManager::Enqueue(const std::string_view uri, const std::string_view name,
  Net::CurlData *data) noexcept {
  assert(download_manager != nullptr);

  download_manager->Enqueue(Java::GetEnv(), uri, name, data);
}

void
Net::DownloadManager::Cancel(const std::string_view name) noexcept
{
  assert(download_manager != nullptr);

  download_manager->Cancel(Java::GetEnv(), name);
}

#else /* !ANDROID */

#include "Init.hpp"
#include "CoDownload.hpp"
#include "lib/curl/Global.hxx"
#include "Operation/ProgressListener.hpp"
#include "LocalPath.hpp"
#include "thread/Mutex.hxx"
#include "thread/SafeList.hxx"
#include "co/InjectTask.hxx"

#include <string>
#include <list>
#include <algorithm>

#include <string.h>

  struct Item {
    std::string uri;
    // AllocatedPath path_relative;
    std::string name;
    const Net::DownloadType type;
    boost::json::value* json;
    boost::json::value  tmp_json;
    Net::CurlData *data;

//    Item(const Item &other) = delete;
    // aug: Copy-constructor...
    Item(const Item &other) : uri(other.uri), name(other.name),
      type(other.type), json(other.json), data(other.data) {}

    Item(Item &&other) noexcept = default;

    Item(const std::string_view _uri, const std::string_view _name,
      const Net::DownloadType _type) noexcept
      :uri(_uri), name(_name), type(_type) , json(&tmp_json), data(nullptr) {}

    Item(const std::string_view _uri, const std::string_view _name,
      Net::CurlData *_data, boost::json::value *_json) noexcept
      :uri(_uri), name(_name), type(Net::JSON), json(_json), data(_data) {}

    Item(const std::string_view _uri, Net::CurlData *_data) noexcept
      :uri(_uri), name(_data->name), type(_data->type), data(_data) {}

    Item(const std::string_view _uri, const Path path_relative) noexcept
      :uri(_uri), name(path_relative.c_str()), type(Net::FILE), data(nullptr) {}

    Item &operator=(const Item &other) = delete;

    [[gnu::pure]]
    bool operator==(std::string_view  other) const noexcept {
      // return path_relative == other;
      return name == other;
    }
  };

  class DownloadManagerThread final
    : ProgressListener {
    /**
   * The coroutine performing the current download.
   */
  Co::InjectTask task{Net::curl->GetEventLoop()};

  Mutex mutex;

  /**
   * Information about the current download, i.e. queue.front().
   * Protected by #mutex.
   */
  int64_t current_size = -1, current_position = -1;

  std::list<Item> queue;

  ThreadSafeList<Net::DownloadListener *> listeners;

public:
  void AddListener(Net::DownloadListener &listener) noexcept {
    listeners.Add(&listener);
  }

  void RemoveListener(Net::DownloadListener &listener) noexcept {
    listeners.Remove(&listener);
  }

  void Enumerate(Net::DownloadListener &listener) noexcept {
    for (auto i = queue.begin(), end = queue.end(); i != end; ++i) {
      const Item &item = *i;

      int64_t size = -1, position = -1;
      if (i == queue.begin()) {
        const std::lock_guard lock{mutex};
        size = current_size;
        position = current_position;
      }

      // listener.OnDownloadAdded(item.name, item.type, size, position);
      listener.OnDownloadAdded(item.name, size, position);
    }
  }

  void 
  Enqueue(const std::string_view uri, Path path_relative) noexcept
  {
    const std::string name = path_relative.c_str();
    queue.emplace_back(uri, LocalPath(path_relative));

    listeners.ForEach([name](auto *listener){
      listener->OnDownloadAdded(name, -1, -1);
    });

    if (!task)
      Start();
  }

  void
    Enqueue(const std::string_view uri, const Path path,
      Net::CurlData *data) noexcept
  {
    if (data) 
        data->name = path.c_str();
    queue.emplace_back(uri, data);
    std::string_view name = path.GetBase().c_str();
    listeners.ForEach([name](auto *listener) {
      listener->OnDownloadAdded(name, -1, -1);
    });

    if (!task)
      Start();
  }
  void
  Enqueue(const std::string_view uri, const std::string_view name,
    boost::json::value *json) noexcept
  {
    queue.emplace_back(uri, name, nullptr, json);
  
    listeners.ForEach([name](auto *listener){
      listener->OnDownloadAdded(name, -1, -1);
    });
  
    if (!task)
      Start();
  }
  
  void
  Enqueue(const std::string_view uri, const std::string_view name,
    Net::CurlData *data) noexcept
  {
    queue.emplace_back(uri, data);
    if (data)
      data->name = name;
    listeners.ForEach([name](auto *listener){
      listener->OnDownloadAdded(name, -1, -1);
    });
  
    if (!task)
      Start();
  }
    
  void
  Cancel(const std::string_view name) noexcept
  {
    auto i = std::find(queue.begin(), queue.end(), name);
    if (i == queue.end())
      return;

    if (i == queue.begin()) {
      /* current download; stop the thread to cancel the current file
         and restart the thread to continue downloading the following
         files */

      task.Cancel();
      current_size = current_position = -1;

      if (!queue.empty())
        Start();
    } else {
      /* queued download; simply remove it from the list */
      queue.erase(i);
    }

    listeners.ForEach([name](auto *listener){
      listener->OnDownloadError(name, {});
    });
  }

private:
  void Start() noexcept;
  void OnCompletion(std::exception_ptr error) noexcept;
  // void OnCompletionJson(std::exception_ptr error) noexcept;

  /* methods from class ProgressListener */
  void SetProgressRange(unsigned range) noexcept override {
    const std::lock_guard lock{mutex};
    current_size = range;
  }

  void SetProgressPosition(unsigned position) noexcept override {
    const std::lock_guard lock{mutex};
    current_position = position;
  }
};

#if 0  // aug!!!!!!!!!!!!!!!!!
static Co::InvokeTask
DownloadToFile(CurlGlobal &curl,
               const std::string_view url, AllocatedPath path,
               std::array<std::byte, 32> *sha256,
               ProgressListener &progress)
{
  const auto ignored_response = co_await
    Net::CoDownloadToFile(curl, url.data(), nullptr, nullptr,
                          path, sha256, progress);
}
static Co::InvokeTask
DownloadToFile2(CurlGlobal &curl,
  const std::string_view url, AllocatedPath path,
  const Net::CurlData *data,
  ProgressListener &progress)
{
  const auto ignored_response = co_await
    Net::CoDownloadToFile(curl, url.data(),
      path, data, progress);
}

static Co::InvokeTask
DownloadToJson3(CurlGlobal &curl,
  const std::string_view url,
  boost::json::value &json_body,
  const Net::CurlData *data,
  ProgressListener &progress)
{
  // const auto 
    json_body = co_await
    Net::CoDownloadToJson(curl, url, data, //username, password, &cred_slist,
                          progress);
}
#endif  // 0 - aug

static Co::InvokeTask
DownloadTask(CurlGlobal &curl,
  const Item *item,
  ProgressListener &progress)
{
  // const auto 
  switch (item->type) {
    case Net::JSON:
      *item->data->json_value = co_await
        Net::CoDownloadToJson(curl, item->uri, std::move(item->data), progress);
      break;
    case Net::FILE: {
#if 1
      auto path = AllocatedPath(item->name);
      const auto ignored_response = co_await
      Net::CoDownloadToFile(curl, item->uri, path, std::move(item->data), progress);
#else
      const auto ignored_response = co_await
        Net::CoDownloadToFile(curl, item->uri.data(),
          AllocatedPath(item->name.data()), item->data, progress);


      // Net::CoDownloadToFile(curl, item->uri, item->data->curl_list, Path(item->name.c_str()),nullptr, progress);
//      auto path = AllocatedPath(item->name.c_str());
//      Net::CoDownloadToFile(curl, item->uri.data(), path, item->data, progress);
//      auto path = AllocatedPath(item->name.c_str());
//      Net::CoDownloadToFile(curl, item->uri.data(), nullptr, nullptr, path, nullptr, progress);
#endif
      }
      break;
    default:
      break;

  }
}
/*
static Co::InvokeTask
DownloadToFile5(CurlGlobal &curl,
  const Item *item,
  ProgressListener &progress)
{
  const auto ignored_response = co_await
    Net::CoDownloadToFile(curl, item->uri.data(),
      AllocatedPath(item->name.data()), item->data, progress);
}

*/

void
DownloadManagerThread::Start() noexcept
{
  if (task)
    return;
  assert(!queue.empty());
  assert(!task);
  assert(current_size == -1);
  assert(current_position == -1);

  // const Item &item = queue.front();
  const Item *item = new Item(queue.front());
  current_position = 0;
  task.Start(DownloadTask(*Net::curl, std::move(item), *this), BIND_THIS_METHOD(OnCompletion));
}

void
DownloadManagerThread::OnCompletion(std::exception_ptr error) noexcept
{
  assert(!queue.empty());

  std::string name = std::move(queue.front().name);
  if (queue.front().type == Net::FILE) {
    name = AllocatedPath(name).GetBase().c_str();
  }
  queue.pop_front();

  current_size = current_position = -1;

  if (error) {
    LogError(error);
    listeners.ForEach([_name=name, &error](auto *listener){
      listener->OnDownloadError(_name, error);
    });
  } else {
    listeners.ForEach([_name = name](auto *listener) {
      listener->OnDownloadComplete(_name);
    });
  }

  // start the next download
  if (!queue.empty())
    Start();
}

static DownloadManagerThread *thread;

bool
Net::DownloadManager::Initialise() noexcept
{
  assert(thread == nullptr);

  thread = new DownloadManagerThread();
  return true;
}

void
Net::DownloadManager::BeginDeinitialise() noexcept
{
}

void
Net::DownloadManager::Deinitialise() noexcept
{
  assert(thread != nullptr);

  delete thread;
  thread = nullptr;
}

bool
Net::DownloadManager::IsAvailable() noexcept
{
  assert(thread != nullptr);

  return true;
}

void
Net::DownloadManager::AddListener(DownloadListener &listener) noexcept
{
  assert(thread != nullptr);

  thread->AddListener(listener);
}

void
Net::DownloadManager::RemoveListener(DownloadListener &listener) noexcept
{
  assert(thread != nullptr);

  thread->RemoveListener(listener);
}

void
Net::DownloadManager::Enumerate(DownloadListener &listener) noexcept
{
  assert(thread != nullptr);

  thread->Enumerate(listener);
}

void
Net::DownloadManager::Enqueue(const std::string_view uri,
  const Path path_relative) noexcept
{
  assert(thread != nullptr);

  thread->Enqueue(uri, path_relative);
}

void
Net::DownloadManager::Enqueue(const std::string_view uri, const std::string_view name,
  boost::json::value &json) noexcept
{
  assert(thread != nullptr);

  thread->Enqueue(uri, name, &json);
}

void
Net::DownloadManager::Enqueue(const std::string_view uri,
  const std::string_view name, Net::CurlData *data) noexcept
{
  assert(thread != nullptr);

  if (data)
    data->name = name;
  thread->Enqueue(uri, name, data);
}

void
Net::DownloadManager::Enqueue(const std::string_view uri,
  const Path path, Net::CurlData *data) noexcept
{
  assert(thread != nullptr);

  if (data)
    data->name = path.c_str();
  thread->Enqueue(uri, path, data);
}

void
Net::DownloadManager::Cancel(const std::string_view name) noexcept
{
  assert(thread != nullptr);

  thread->Cancel(name);
}

#endif
