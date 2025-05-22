// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#ifdef NO_ANDROID  // aug: new

#include "DownloadManager.hpp"
#include "Main.hpp"
#include "net/http/DownloadManager.hpp"
#include "net/http/CoDownload.hpp"  // aug because Net::CurlData
#include "Context.hpp"
#include "java/Class.hxx"
#include "java/Env.hxx"
#include "java/Path.hxx"
#include "java/String.hxx"
#include "LocalPath.hpp"
#include "io/CopyFile.hxx"
#include "util/Macros.hpp"
#include "util/StringAPI.hxx"
#include "de_opensoar_DownloadUtil.h"

#include <algorithm>

static Java::TrivialClass util_class;

static jmethodID ctor, enumerate_method, enqueue_method, cancel_method;

static Java::LocalObject
NewDownloadUtil(JNIEnv *env, AndroidDownloadManager &instance, Context &context)
{
  return Java::NewObjectRethrow(env, util_class, ctor,
                                (jlong)(std::size_t)&instance,
                                context.Get());
}

AndroidDownloadManager::AndroidDownloadManager(JNIEnv *env,
                                               Context &context)
  :util(env, NewDownloadUtil(env, *this, context))
{
}

bool
AndroidDownloadManager::Initialise(JNIEnv *env) noexcept
{
  assert(util_class == nullptr);
  assert(env != nullptr);

  if (!util_class.FindOptional(env, "de/opensoar/DownloadUtil"))
    return false;

  ctor = env->GetMethodID(util_class, "<init>",
                          "(JLandroid/content/Context;)V");
  if (Java::DiscardException(env)) {
    /* need to check for Java exceptions again because the first
       method lookup initializes the Java class */
    util_class.Clear(env);
    return false;
  }

  enumerate_method = env->GetMethodID(util_class, "enumerate", "(J)V");

  enqueue_method = env->GetMethodID(util_class, "enqueue",
                                    "(Ljava/lang/String;Ljava/lang/String;)J");

  cancel_method = env->GetMethodID(util_class, "cancel",
                                   "(Ljava/lang/String;)V");

  return true;
}

void
AndroidDownloadManager::Deinitialise(JNIEnv *env) noexcept
{
  util_class.ClearOptional(env);
}

bool
AndroidDownloadManager::IsAvailable() noexcept
{
  return util_class.Get() != nullptr;
}

void
AndroidDownloadManager::AddListener(Net::DownloadListener &listener) noexcept
{
  const std::lock_guard lock{mutex};

  assert(std::find(listeners.begin(), listeners.end(),
                   &listener) == listeners.end());

  listeners.push_back(&listener);
}

void
AndroidDownloadManager::RemoveListener(Net::DownloadListener &listener) noexcept
{
  const std::lock_guard lock{mutex};

  auto i = std::find(listeners.begin(), listeners.end(), &listener);
  assert(i != listeners.end());
  listeners.erase(i);
}

void
// AndroidDownloadManager::OnDownloadComplete(Path path_relative,
AndroidDownloadManager::OnDownloadComplete(const std::string_view name,
                                           bool success) noexcept
{
  const std::lock_guard lock{mutex};

  if (success)
    for (auto i = listeners.begin(), end = listeners.end(); i != end; ++i)
      (*i)->OnDownloadComplete(name.data());
  else
    for (auto i = listeners.begin(), end = listeners.end(); i != end; ++i)
      // TODO obtain error details
      (*i)->OnDownloadError(name.data(), {});
}

JNIEXPORT void JNICALL
Java_de_opensoar_DownloadUtil_onDownloadAdded(JNIEnv *env, [[maybe_unused]] jobject obj,
                                             jlong j_handler, jstring j_path,
                                             jlong size, jlong position)
{
  const auto relative_path = Java::ToPath(env, j_path);

  Net::DownloadListener &handler = *(Net::DownloadListener *)(size_t)j_handler;
  handler.OnDownloadAdded(relative_path.c_str(), size, position);
}

JNIEXPORT void JNICALL
Java_de_opensoar_DownloadUtil_onDownloadComplete(JNIEnv *env, [[maybe_unused]] jobject obj,
                                                jlong ptr,
                                                jstring j_tmp_path,
                                                jstring j_relative_path,
                                                jboolean success)
{
  auto &dm = *(AndroidDownloadManager *)(size_t)ptr;

  const auto tmp_path = Java::ToPath(env, j_tmp_path);
  const auto relative_path = Java::ToPath(env, j_relative_path);

  const auto final_path = LocalPath(relative_path);

  if (success) {
    try {
      MoveOrCopyFile(tmp_path, final_path);
    } catch (...) {
      success = false;
    }
  }

// aug   dm.OnDownloadComplete(relative_path, success);
  dm.OnDownloadComplete(relative_path.c_str(), success);
}

void
AndroidDownloadManager::Enumerate(JNIEnv *env,
                                  Net::DownloadListener &listener) noexcept
{
  assert(env != nullptr);

  env->CallVoidMethod(util, enumerate_method,
                      (jlong)(size_t)&listener);
}

void
AndroidDownloadManager::Enqueue(JNIEnv *env, const std::string_view uri,
  Path path_relative) noexcept
{
  assert(env != nullptr);
  assert(!uri.empty());
  assert(path_relative != nullptr);

  Java::String j_uri(env, uri.data());
  Java::String j_path(env, path_relative.c_str());

  env->CallLongMethod(util, enqueue_method,
    j_uri.Get(), j_path.Get());

  try {
    /* the method DownloadManager.enqueue() can throw
       SecurityException if Android doesn't like the destination path
       ("Unsupported path") */
    Java::RethrowException(env);
  }
  catch (...) {
    const auto error = std::current_exception();

    const std::lock_guard lock{ mutex };
    for (auto *i : listeners)
    //  i->OnDownloadError(path_relative, error);
    // aug      
      i->OnDownloadError(path_relative.c_str(), error);
      // i->OnDownloadError(name.data(), error);
    return;
  }

  const std::lock_guard lock{ mutex };
  for (auto i = listeners.begin(), end = listeners.end(); i != end; ++i)
    // aug    (*i)->OnDownloadAdded(path_relative, -1, -1);
    (*i)->OnDownloadAdded(path_relative.c_str(), -1, -1);
    // (*i)->OnDownloadAdded(name.data(), -1, -1);
}

void
AndroidDownloadManager::Enqueue(JNIEnv *env, const std::string_view uri,
                                [[maybe_unused]] const std::string_view name,
  [[maybe_unused]] boost::json::value &json) noexcept
{
  assert(env != nullptr);
  assert(!uri.empty());
  assert(!name.empty());

  [[maybe_unused]] Java::String j_uri(env, uri.data());
#if 0
  // Android get json without path...
  // aug: Möglicherweise gar nicht mit Java notwendig???

//  Java::String j_path(env, path_relative.c_str());
  Java::String j_path(env, name.data());

  env->CallLongMethod(util, enqueue_method,
                      j_uri.Get(), j_path.Get());

  try {
    /* the method DownloadManager.enqueue() can throw
       SecurityException if Android doesn't like the destination path
       ("Unsupported path") */
    Java::RethrowException(env);
  } catch (...) {
    const auto error = std::current_exception();

    const std::lock_guard lock{mutex};
    for (auto *i : listeners)
// aug      i->OnDownloadError(path_relative, error);
//      i->OnDownloadError(path_relative.c_str(), error);
      i->OnDownloadError(name.data(), error);
    return;
  }

  const std::lock_guard lock{mutex};
  for (auto i = listeners.begin(), end = listeners.end(); i != end; ++i)
    // aug    (*i)->OnDownloadAdded(path_relative, -1, -1);
    // (*i)->OnDownloadAdded(path_relative.c_str(), -1, -1);
    (*i)->OnDownloadAdded(name.data(), -1, -1);
#endif
}

void
AndroidDownloadManager::Enqueue(JNIEnv *env, const std::string_view uri,
  const std::string_view name, [[maybe_unused]] Net::CurlData *data) noexcept
{
  assert(env != nullptr);
  assert(!uri.empty());
  assert(!name.empty());

  [[maybe_unused]] Java::String j_uri(env, uri);
  // [[maybe_unused]] Java::String j_uri(env, uri.data());
#if 0
  // Android get json without path...
  // aug: Möglicherweise gar nicht mit Java notwendig???
#endif
}

void
// AndroidDownloadManager::Cancel(JNIEnv *env, Path path_relative) noexcept
AndroidDownloadManager::Cancel(JNIEnv *env, std::string_view name) noexcept
{
  assert(env != nullptr);
//  assert(path_relative != nullptr);
  assert(!name.empty());

//  Java::String j_path(env, path_relative.c_str());
//  Java::String j_path(env, name.data());
  Java::String j_path(env, name);
  env->CallVoidMethod(util, cancel_method, j_path.Get());
}

#endif  // ifdef NO_ANDROID
