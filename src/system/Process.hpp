// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <filesystem>

/**
 * Launch a child process but don't wait for it to exit.
 */
bool
Start(const char *const*argv) noexcept;

template<typename... Args>
static inline bool
Start(const char *path, Args... args) noexcept
{
  const char *const argv[]{path, args..., nullptr};
  return Start(argv);
}

/**
 * Launch a child process and wait for it to exit.
 */
class Path;

int
Run(const char *const*argv) noexcept;
int 
Run(const Path &output, const char *const *argv) noexcept;

template<typename... Args>
static inline int
Run(const char *path, Args... args) noexcept
{
  const char *const argv[]{path, args..., nullptr};
  return Run(argv);
}

template<typename... Args>
static inline int
Run(const Path &output, const char *path, Args... args) noexcept
{
  const char *const argv[]{path, args..., nullptr};
  return Run(output, argv);
}

// int 
// Run(const Path &output, const char *path, ...) noexcept;

// static inline 
// int
// RunX(const std::filesystem::path output, const char *path) noexcept
// {

// int
// RunX(const Path &output, const char *path) noexcept
// {
//   const char *const argv[]{path, nullptr};
//   return Run(output, argv);
// }
