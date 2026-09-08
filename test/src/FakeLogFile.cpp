// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "LogFile.hpp"
#include "FakeLogFile.hpp"
#include "util/Exception.hxx"

#include <fmt/format.h>

#include <exception>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <iterator>

#ifdef _WIN32
#include <io.h>
#define ISATTY_STDERR() _isatty(_fileno(stderr))
#else
#include <unistd.h>
#define ISATTY_STDERR() isatty(STDERR_FILENO)
#endif

/**
 * Log output belongs to a person, not to a test harness: a program
 * started by hand in a terminal prints, one whose stderr goes into a
 * pipe (prove, make check) stays quiet - negative test cases would
 * fill the report with error messages that are expected and boring.
 * VERBOSE=1 forces printing, VERBOSE=0 forces silence.
 */
static bool
DefaultQuiet() noexcept
{
  if (const char *v = getenv("VERBOSE"); v != nullptr)
    return *v == '0';

  return !ISATTY_STDERR();
}

static bool quiet = DefaultQuiet();

void
SetFakeLogFileQuiet(bool _quiet) noexcept
{
  quiet = _quiet;
}

void
LogString(std::string_view s) noexcept
{
  if (quiet)
    return;

  fprintf(stderr, "%.*s\n",
          int(s.size()), s.data());
}

void
LogVFmt(fmt::string_view format_str, fmt::format_args args) noexcept
{
	fmt::memory_buffer buffer;
#if FMT_VERSION >= 80000
	fmt::vformat_to(std::back_inserter(buffer), format_str, args);
#else
	fmt::vformat_to(buffer, format_str, args);
#endif
	LogString({buffer.data(), buffer.size()});
}

void
LogFormat(const char *fmt, ...) noexcept
{
  if (quiet)
    return;

  va_list ap;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  fputc('\n', stderr);
}

void
LogError(std::exception_ptr e) noexcept
{
  LogFormat("%s", GetFullMessage(e).c_str());
}

void
LogError(std::exception_ptr e, const char *msg) noexcept
{
  LogFormat("%s: %s", msg, GetFullMessage(e).c_str());
}
