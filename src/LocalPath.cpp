// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "LocalPath.hpp"
#include "ProductName.hpp"
#include "system/Path.hpp"
#include "Compatibility/path.h"
#include "util/StringCompare.hxx"
#include "util/StringFormat.hpp"
#include "util/StringAPI.hxx"
#include "Asset.hpp"

#ifdef __APPLE__
#include "Apple/PathProvider.hpp"
#endif

#include "system/FileUtil.hpp"

#ifdef ANDROID
#include "Android/Context.hpp"
#include "Android/Environment.hpp"
#include "Android/Main.hpp"
#endif

#ifdef _WIN32
#include "system/UTF8Win32.hpp"
#endif

#include <algorithm>
#include <list>
#include <string>
#include <string_view>
#include <stdio.h>

#include <cassert>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <shlobj.h>
#include <windef.h> // for MAX_PATH
#include "system/Win32UTF8PathGuard.hpp"
#endif

#ifdef ANDROID
#include <android/log.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

/**
 * This is the partition that the Kobo software mounts on PCs
 */
#define KOBO_USER_DATA "/mnt/onboard"

/**
 * A list of product data directories.  The first one is the primary
 * one, where "%LOCAL_PATH%\\" refers to.
 */
static std::list<AllocatedPath> data_paths;

static AllocatedPath cache_path;

/**
 * Settings that belong to the device, not to a profile or a data
 * directory: deliberately outside the data path (and not below it),
 * so that copying or replacing the data directory does not carry
 * them along.
 */
static AllocatedPath system_config_path;

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
LocalPath(const char *file) noexcept
{
  return LocalPath(Path(file));
}

AllocatedPath
MakeLocalPath(const char *name)
{
  auto path = LocalPath(name);
  Directory::Create(path);
  return path;
}

AllocatedPath
MakeLocalPath(const Path name)
{
  return MakeLocalPath(name.c_str());
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
 * Replace the final path component of a UTF-8 path string.
 */
static void
ReplaceBaseNameUTF8(std::string &path, const char *new_base) noexcept
{
  const auto slash = path.find_last_of("/\\");
  if (slash == std::string::npos)
    path = new_base;
  else
    path.replace(slash + 1, std::string::npos, new_base);
}

/**
 * Find a product data folder in the same location as the executable.
 */
[[gnu::pure]]
static AllocatedPath
FindDataPathAtModule(HMODULE hModule) noexcept
{
  wchar_t buffer[MAX_PATH];
  const DWORD n = GetModuleFileNameW(hModule, buffer, MAX_PATH);
  /* n == MAX_PATH means truncated (and may lack a terminator). */
  if (n == 0 || n >= MAX_PATH)
    return nullptr;

  std::string path = WideToUTF8(std::wstring_view(buffer, n));
  if (path.empty())
    return nullptr;

  ReplaceBaseNameUTF8(path, PRODUCT_DATA_DIR);
  return Directory::Exists(Path(path.c_str()))
    ? AllocatedPath(path.c_str())
    : nullptr;
}

#endif /* _WIN32 */

static std::list<AllocatedPath>
FindDataPaths() noexcept
{
  std::list<AllocatedPath> result;

  /* Kobo: hard-coded product data path */
  if constexpr (IsKobo()) {
    result.emplace_back(KOBO_USER_DATA DIR_SEPARATOR_S PRODUCT_DATA_DIR);
    return result;
  }

  /* Android: ask the Android API */
  if constexpr (IsAndroid()) {
#ifdef ANDROID
    const auto env = Java::GetEnv();

    bool external_files_dirs_path_added = false;
    for (auto &path : context->GetExternalFilesDirs(env)) {
      __android_log_print(ANDROID_LOG_DEBUG, "XCSoar",
                          "Context.getExternalFilesDirs()='%s'",
                          path.c_str());
      auto xcsoarlog_path = AllocatedPath::Build(Path(path), Path("xcsoar.log"));
      if(File::Exists(xcsoarlog_path)) {
        /*
         * Old Android user will keep using getExternalFilesDirs() if they already have data in it
         * Otherwise we should default them to the new getExternalMediaDirs
         * 
         * This is for backward compatibility so user won't surprise when
         * all their config suddenly gone after upgrade the app
         */
        __android_log_print(ANDROID_LOG_DEBUG, "XCSoar",
          "Found xcsoar.log in '%s', keep using Android private storage.",
          xcsoarlog_path.c_str());
        result.emplace_back(std::move(path));
        external_files_dirs_path_added = true;
      }
    }

    if(!external_files_dirs_path_added) {
      for (auto &path : context->GetExternalMediaDirs(env)) {
        __android_log_print(ANDROID_LOG_DEBUG, "XCSoar",
                            "Context.getExternalMediaDirs()='%s'",
                            path.c_str());
        result.emplace_back(std::move(path));
      }
    }

    if (auto path = Environment::GetExternalStoragePublicDirectory(env,
                                                                   PRODUCT_DATA_DIR);
        path != nullptr) {
      const bool writable = access(path.c_str(), W_OK) == 0;

      __android_log_print(ANDROID_LOG_DEBUG, "XCSoar",
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
  /* look for a product data directory in the same directory as
     the executable */
  if (auto path = FindDataPathAtModule(nullptr); path != nullptr)
    result.emplace_back(std::move(path));

  /* Windows: use "My Documents\<ProductDataDir>" */
  {
    wchar_t buffer[MAX_PATH];
    if (SHGetSpecialFolderPathW(nullptr, buffer, CSIDL_PERSONAL,
                                result.empty())) {
      const std::string personal = WideToUTF8(buffer);
      if (!personal.empty())
        result.emplace_back(AllocatedPath::Build(personal.c_str(),
                                                 PRODUCT_DATA_DIR));
    }
  }
#endif // _WIN32

#ifdef HAVE_POSIX
  /* on Unix, use ~/.<product_name> */
  if (const char *home = getenv("HOME"); home != nullptr) {
#ifdef __APPLE__
    /* macOS users are not used to dot-files in their home
       directory - make it a little bit easier for them to find the
       files.  If target is an iOS device, use the already existing
       "Documents" folder inside the application's sandbox.  This
       folder can also be accessed via iTunes, if
       UIFileSharingEnabled is set to YES in Info.plist */
    const Path in_home = Apple::GetDataPathInHome();
#else // !APPLE
    constexpr const char *in_home = PRODUCT_UNIX_HOME_DIR;
#endif

    result.emplace_back(AllocatedPath::Build(Path(home), in_home));
#ifdef __APPLE__
    const Path data_path(result.back().c_str());
    if (!Apple::EnsureDataPathExists(data_path)) {
      const std::string utf8_path = data_path.ToUTF8();
      if (!utf8_path.empty())
        fprintf(stderr, "Failed to create data path '%s'\n",
                utf8_path.c_str());
      else
        fprintf(stderr, "Failed to create data path (unknown path)\n");
    }
#endif
  }

#ifndef __APPLE__
  /* Linux (and others): allow global configuration in /etc/<product_name> */
  if (Directory::Exists(Path{PRODUCT_UNIX_SYSCONF_DIR}))
    result.emplace_back(Path{PRODUCT_UNIX_SYSCONF_DIR});
#endif // !APPLE
#endif // HAVE_POSIX

  return result;
}

void
VisitDataFiles(const char* filter, File::Visitor &visitor)
{
  for (const auto &i : data_paths)
    Directory::VisitSpecificFiles(i, filter, visitor, true);
}

/**
 * Where the device keeps settings that must not travel with the data
 * directory.  Each platform has a place for this; the data path is
 * only the last resort, and even then it is a hidden sibling
 * directory, never a subdirectory.
 */
static AllocatedPath
FindSystemConfigPath() noexcept
{
#ifdef ANDROID
  /* private internal storage: invisible to the user, kept when the
     cache is cleared */
  if (auto path = context->GetFilesDir(Java::GetEnv()); path != nullptr)
    return path;
#elif defined(KOBO)
  /* next to the data directory on the partition the user sees */
  return AllocatedPath{Path{KOBO_USER_DATA DIR_SEPARATOR_S "." PRODUCT_NAME_LC}};
#elif defined(_WIN32)
  {
    wchar_t buffer[MAX_PATH];
    if (SHGetSpecialFolderPathW(nullptr, buffer, CSIDL_LOCAL_APPDATA, true)) {
      const std::string local_app_data = WideToUTF8(buffer);
      if (!local_app_data.empty())
        return AllocatedPath::Build(local_app_data.c_str(), PRODUCT_NAME);
    }
  }
#elif defined(HAVE_POSIX)
  if (const char *xdg = getenv("XDG_CONFIG_HOME");
      xdg != nullptr && *xdg != '\0') {
    const std::string dir = std::string{xdg} + DIR_SEPARATOR_S PRODUCT_NAME_LC;
    return AllocatedPath{Path{dir.c_str()}};
  }

  if (const char *home = getenv("HOME"); home != nullptr && *home != '\0') {
    const std::string dir = std::string{home}
      + DIR_SEPARATOR_S ".config" DIR_SEPARATOR_S PRODUCT_NAME_LC;
    return AllocatedPath{Path{dir.c_str()}};
  }
#endif

  /* last resort: a hidden directory beside the data directory */
  if (const auto parent = GetPrimaryDataPath().GetParent(); parent != nullptr)
    return AllocatedPath::Build(parent, Path{"." PRODUCT_NAME_LC});

  return nullptr;
}

/**
 * Where do the cache files go?
 *
 * Not into the data directory: that one belongs to the user, who
 * copies and backs it up, and temporary files have no business being
 * carried along.  Each product gets its own directory, because the
 * contents differ - the SkySight files of XCSoar and OpenSoar do not
 * look alike.  On the OpenVario the place is fixed on the data
 * partition and deliberately does not follow -datapath=: the root
 * filesystem is small and must not fill up with cache files.
 */
static AllocatedPath
FindCachePath() noexcept
{
#if defined(IS_OPENVARIO)
  /* the data partition is mounted as 'data' in the home directory.  The
     product directory below it is not needed today, since XCSoar still
     keeps its cache in its data directory, but it is where the same rule
     would put it once that changes - better here from the start than
     moved later. */
  if (const char *home = getenv("HOME"); home != nullptr && *home != '\0') {
    const std::string dir = std::string{home}
      + DIR_SEPARATOR_S "data" DIR_SEPARATOR_S ".cache" DIR_SEPARATOR_S PRODUCT_NAME;
    return AllocatedPath{Path{dir.c_str()}};
  }
#elif defined(KOBO)
  /* unchanged: the Kobo keeps its cache in the data directory */
#elif defined(_WIN32)
  {
    wchar_t buffer[MAX_PATH];
    if (SHGetSpecialFolderPathW(nullptr, buffer, CSIDL_LOCAL_APPDATA, true)) {
      const std::string local_app_data = WideToUTF8(buffer);
      if (!local_app_data.empty())
        return AllocatedPath::Build(local_app_data.c_str(),
                                    PRODUCT_NAME DIR_SEPARATOR_S ".cache");
    }
  }
#elif defined(HAVE_POSIX)
  if (const char *xdg = getenv("XDG_CACHE_HOME");
      xdg != nullptr && *xdg != '\0') {
    const std::string dir = std::string{xdg} + DIR_SEPARATOR_S PRODUCT_NAME;
    return AllocatedPath{Path{dir.c_str()}};
  }

  if (const char *home = getenv("HOME"); home != nullptr && *home != '\0') {
    const std::string dir = std::string{home}
      + DIR_SEPARATOR_S ".cache" DIR_SEPARATOR_S PRODUCT_NAME;
    return AllocatedPath{Path{dir.c_str()}};
  }
#endif

  /* last resort: inside the data directory, where it used to live */
  return LocalPath("cache");
}

Path
GetCachePath() noexcept
{
  return cache_path;
}

AllocatedPath
GetUserDownloadsPath() noexcept
{
#if defined(IS_OPENVARIO_CB2)
  /* the device: a fixed directory on the data partition, which the
     wrapper scripts and the USB transfer know as well */
  if (const char *home = getenv("HOME"); home != nullptr && *home != '\0')
    return AllocatedPath::Build(Path{home}, Path{"data" DIR_SEPARATOR_S "download"});
  return nullptr;
#elif defined(ANDROID)
  const auto env = Java::GetEnv();
  return Environment::GetExternalStoragePublicDirectory(env, "Download");
#elif defined(_WIN32)
  /* FOLDERID_Downloads, spelled out so that no uuid library has to be
     linked for one GUID */
  static constexpr GUID folderid_downloads =
    {0x374DE290, 0x123F, 0x4565, {0x91, 0x64, 0x39, 0xC4, 0x92, 0x5E, 0x46, 0x7B}};

  PWSTR wide = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(folderid_downloads, 0, nullptr, &wide)) &&
      wide != nullptr) {
    const std::string utf8 = WideToUTF8(wide);
    CoTaskMemFree(wide);
    if (!utf8.empty())
      return AllocatedPath{Path{utf8.c_str()}};
  }
  return nullptr;
#else
  const char *home = getenv("HOME");
  if (home == nullptr || *home == '\0')
    return nullptr;

  /* the XDG user directory, if the desktop configured one; the file
     holds lines like XDG_DOWNLOAD_DIR="$HOME/Downloads" */
  {
    const auto config = AllocatedPath::Build(Path{home},
                                             Path{".config" DIR_SEPARATOR_S "user-dirs.dirs"});
    std::string dir;
    if (FILE *file = fopen(config.c_str(), "r"); file != nullptr) {
      char line[512];
      while (fgets(line, sizeof(line), file) != nullptr) {
        std::string_view v{line};
        if (!v.starts_with("XDG_DOWNLOAD_DIR="))
          continue;

        v.remove_prefix(strlen("XDG_DOWNLOAD_DIR="));
        while (!v.empty() && (v.back() == '\n' || v.back() == '\r' || v.back() == '"'))
          v.remove_suffix(1);
        if (v.starts_with('"'))
          v.remove_prefix(1);

        if (v.starts_with("$HOME")) {
          dir = home;
          v.remove_prefix(strlen("$HOME"));
        }
        dir.append(v);
        break;
      }
      fclose(file);
    }

    if (!dir.empty())
      return AllocatedPath{Path{dir.c_str()}};
  }

  return AllocatedPath::Build(Path{home}, Path{"Downloads"});
#endif
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
  // If data_paths is already set (e.g., by -datapath= command line option),
  // don't overwrite it with default paths
  if (data_paths.empty()) {
    data_paths = FindDataPaths();
    if (data_paths.empty())
      throw std::runtime_error("No data path found");
  }

#ifdef ANDROID
  cache_path = context->GetExternalCacheDir(Java::GetEnv());
  if (cache_path == nullptr)
    throw std::runtime_error("No Android cache directory");

  // TODO: delete the old cache directory in product data directory?
#else
  cache_path = FindCachePath();
#endif

  system_config_path = FindSystemConfigPath();
}

void
DeinitialiseDataPath() noexcept
{
  data_paths.clear();
}

Path
GetSystemConfigPath() noexcept
{
  return system_config_path;
}

Path
MakeSystemConfigPath() noexcept
{
  if (system_config_path == nullptr)
    return nullptr;

  try {
    Directory::Create(system_config_path);
  } catch (...) {
    return nullptr;
  }

  return system_config_path;
}

void
CreateDataPath()
{
  Directory::Create(GetPrimaryDataPath());
}
