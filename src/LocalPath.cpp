// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "LocalPath.hpp"
#include "system/Path.hpp"
#include "Compatibility/path.h"
#include "util/StringCompare.hxx"
#include "util/StringFormat.hpp"
#include "util/StringAPI.hxx"
#include "Asset.hpp"

#include "system/FileUtil.hpp"

#ifdef ANDROID
#include "Android/Context.hpp"
#include "Android/Environment.hpp"
#include "Android/Main.hpp"
#endif

#ifdef _WIN32
#include "util/UTF8.hpp"
#include "system/PathName.hpp"
#endif

#include <cassert>
#include <stdlib.h>
#ifdef _WIN32
#include <shlobj.h>
#include <windef.h> // for MAX_PATH
#endif

#ifdef ANDROID
#include <android/log.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <string>
#include <algorithm>
#include <list>

#ifdef __APPLE__
#import <Foundation/Foundation.h>
#endif

#include "LogFile.hpp"

#define OPENSOAR_DATADIR "OpenSoarData"

/**
 * This is the partition that the Kobo software mounts on PCs
 */
#define KOBO_USER_DATA "/mnt/onboard"

/**
 * A list of OpenSoarData directories.  The first one is the primary
 * one, where "%LOCAL_PATH%\\" refers to.
 */
static std::list<AllocatedPath> data_paths;

static AllocatedPath cache_path;
static AllocatedPath home_path;

Path
GetPrimaryDataPath() noexcept
{
  assert(!data_paths.empty());

  return data_paths.front();
}

void
SetPrimaryDataPath(Path path) noexcept
{
  assert(path != nullptr);
  assert(!path.empty());

  if (auto i = std::find(data_paths.begin(), data_paths.end(), path);
      i != data_paths.end())
    data_paths.erase(i);

  data_paths.emplace_front(path);
}

void
SetSingleDataPath(Path path) noexcept
{
  assert(path != nullptr);
  assert(!path.empty());

  data_paths.clear();
  data_paths.emplace_front(path);
}

AllocatedPath
LocalPath(Path file) noexcept
{
  assert(file != nullptr);

  return AllocatedPath::Build(GetPrimaryDataPath(), file);
}

AllocatedPath
LocalPath(const std::string_view file) noexcept
{
  return LocalPath(Path(file.data()));
}

AllocatedPath
MakeLocalPath(const char *name)
{
  auto path = LocalPath(name);
  Directory::Create(path);
  return path;
}

Path
RelativePath(Path path) noexcept
{
  return path.RelativeTo(GetPrimaryDataPath());
}

static constexpr char local_path_code[] = "%LOCAL_PATH%\\";

[[gnu::pure]]
static const char *
AfterLocalPathCode(const char *p) noexcept
{
  p = StringAfterPrefix(p, local_path_code);
  if (p == nullptr)
    return nullptr;

  while (*p == '/' || *p == '\\')
    ++p;

  if (StringIsEmpty(p))
    return nullptr;

  return p;
}

AllocatedPath
ExpandLocalPath(Path src) noexcept
{
  // Get the relative file name and location (ptr)
  const char *ptr = AfterLocalPathCode(src.c_str());
  if (ptr == nullptr)
    return src;

#ifndef _WIN32
  // Convert backslashes to slashes on platforms where it matters
  std::string src2(ptr);
  std::replace(src2.begin(), src2.end(), '\\', '/');
  ptr = src2.c_str();
#endif

  // Replace the code "%LOCAL_PATH%\\" by the full local path (output)
  return LocalPath(ptr);
}

AllocatedPath
ContractLocalPath(Path src) noexcept
{
  // Get the relative file name and location (ptr)
  const Path relative = RelativePath(src);
  if (relative == nullptr)
    return nullptr;

  // Replace the full local path by the code "%LOCAL_PATH%\\" (output)
  return Path(local_path_code) + relative.c_str();
}

#ifdef _WIN32

/**
 * Find a OpenSoarData folder in the same location as the executable.
 */
[[gnu::pure]]
static AllocatedPath
FindDataPathAtModule(HMODULE hModule) noexcept
{
  char buffer[MAX_PATH];
  if (GetModuleFileName(hModule, buffer, MAX_PATH) <= 0)
    return nullptr;

  ReplaceBaseName(buffer, OPENSOAR_DATADIR);
  return Directory::Exists(Path(buffer))
    ? AllocatedPath(buffer)
    : nullptr;
}

#endif /* _WIN32 */

static std::list<AllocatedPath>
FindDataPaths() noexcept
{
  std::list<AllocatedPath> result;

  /* Kobo: hard-coded OpenSoarData path */
  if constexpr (IsKobo()) {
    result.emplace_back(KOBO_USER_DATA DIR_SEPARATOR_S OPENSOAR_DATADIR);
    return result;
  }

  /* Android: ask the Android API */
  if constexpr (IsAndroid()) {
#ifdef ANDROID
    const auto env = Java::GetEnv();

    for (auto &path : context->GetExternalMediaDirs(env)) {
      __android_log_print(ANDROID_LOG_DEBUG, "OpenSoar",
                          "Context.getExternalMediaDirs()='%s'",
                          path.c_str());
      result.emplace_back(std::move(path));
    }

    if (auto path = Environment::GetExternalStoragePublicDirectory(env,
                                                                   "OpenSoarData");
        path != nullptr) {
      const bool writable = access(path.c_str(), W_OK) == 0;

      __android_log_print(ANDROID_LOG_DEBUG, "OpenSoar",
                          "Environment.getExternalStoragePublicDirectory()='%s'%s",
                          path.c_str(),
                          writable ? "" : " (not accessible)");

      if (writable)
        /* the "legacy" external storage directory is writable (either
           because this is Android 10 or older, or because the
           "preserveLegacyExternalStorage" is still in effect) - we
           can use it */
        result.emplace_back(std::move(path));
    }
#endif

    return result;
  }

#ifdef _WIN32
  /* look for a OpenSoarData directory in the same directory as
     OpenSoar.exe */
  if (auto path = FindDataPathAtModule(nullptr); path != nullptr)
    result.emplace_back(std::move(path));

  /* Windows: use "My Documents\OpenSoarData" */
  {
    char buffer[MAX_PATH];
    if (SHGetSpecialFolderPath(nullptr, buffer, CSIDL_PERSONAL,
      result.empty()))
    {
      std::string text = buffer;
      std::replace(text.begin(), text.end(), '\\', '/'); 
      result.emplace_back(AllocatedPath::Build(text.data(), OPENSOAR_DATADIR));
    }
  }
#endif // _WIN32

#ifdef HAVE_POSIX
  /* on Unix, use ~/OpenSoarData too */
  if (const char *home = getenv("HOME"); home != nullptr) {
    home_path = AllocatedPath(home);
#ifdef __APPLE__
    /* macOS users are not used to dot-files in their home
       directory - make it a little bit easier for them to find the
       files.  If target is an iOS device, use the already existing
       "Documents" folder inside the application's sandbox.  This
       folder can also be accessed via iTunes, if
       UIFileSharingEnabled is set to YES in Info.plist */
#if (TARGET_OS_IPHONE)
    constexpr const char *in_home = "Documents/" OPENSOAR_DATADIR;
#else
    constexpr const char *in_home = OPENSOAR_DATADIR;
#endif
#else // !APPLE
# ifdef IS_OPENVARIO
    /* OpenVario has a 3rd partition on sdcard, mounted as 'data' */
    constexpr const char *in_home = "data/" OPENSOAR_DATADIR;
# else
    constexpr const char *in_home = OPENSOAR_DATADIR;
# endif
#endif

    result.emplace_back(AllocatedPath::Build(Path(home), in_home));
  }

#ifndef __APPLE__
  /* Linux (and others): allow global configuration in /etc/opensoar */
  if (Directory::Exists(Path{"/etc/opensoar"}))
    result.emplace_back(Path{"/etc/opensoar"});
#else
  if (!Directory::Exists(Path{result.back()})) {
    id fileManager = [NSFileManager defaultManager];
      [fileManager createDirectoryAtPath:[NSString stringWithCString:result.back().c_str()] withIntermediateDirectories:YES attributes:nil error:nil];
  }
#endif // !APPLE
#endif // HAVE_POSIX

  return result;
}

void
VisitDataFiles(const char* filter, File::Visitor &visitor, bool recursive)
{
  for (const auto &i : data_paths)
    Directory::VisitSpecificFiles(i, filter, visitor, recursive);
}

Path
GetCachePath() noexcept
{
  return cache_path;
}

AllocatedPath
GetCachePath(std::string_view path_name) noexcept
{
  if (cache_path == nullptr)
    InitialiseDataPath();
  return AllocatedPath::Build(cache_path, path_name.data());
}

AllocatedPath
MakeCacheDirectory(const char *name) noexcept
{
  Directory::Create(cache_path);
  auto path = AllocatedPath::Build(cache_path, Path(name));
  Directory::Create(path);
  return path;
}

void
InitialiseDataPath()
{
  if (data_paths.empty()) {
    data_paths = FindDataPaths();
    if (data_paths.empty())
      throw std::runtime_error("No data path found");
  }
  // TODO: delete the old cache directory in OpenSoarData?
  if (cache_path == nullptr) {
#ifdef ANDROID
    cache_path = context->GetExternalCacheDir(Java::GetEnv());

#elif defined( _WIN32)  // Windows: Win32, Win64
    PWSTR path = nullptr;
    HRESULT hres = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path);
    if (SUCCEEDED(hres)) {
      /* cache path inside LocalAppData(e.g.
       * 'C:/Users/${USER}/AppData/Local/OpenSoar/.cache' ) */
      std::string str = WideToUTF8(path);
      CoTaskMemFree(path);
      std::replace(str.begin(), str.end(), '\\', '/');
      cache_path = AllocatedPath::Build(str.c_str(), "OpenSoar/.cache");
    } else {
      // cache path inside the data path
      cache_path = LocalPath(".cache");
    }
#elif defined(HAVE_POSIX)
    // OpenVario: own folder of 3rd partition '~/data/.cache'
    // Linux and others: ~/.cache
    cache_path = AllocatedPath::Build(home_path, ".cache");
#endif
    if (cache_path == nullptr)
      throw std::runtime_error("Cache directory not available");
    else {
      Directory::Create(cache_path);
    }

#ifndef TESTING_APP
    LogFmt("Cache path: {}", cache_path.c_str());
    // not allowed (gcc) LogFmt("Cache path:  {}", cache_path.c_str());
#endif
  }
}

void
DeinitialiseDataPath() noexcept
{
  data_paths.clear();
}

void
CreateDataPath()
{
  Directory::Create(GetPrimaryDataPath());
}
