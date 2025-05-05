// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Logger/NMEALogger.hpp"
#include "io/FileOutputStream.hxx"
#include "LocalPath.hpp"
#include "time/BrokenDateTime.hpp"
#include "system/Path.hpp"
#include "util/SpanCast.hxx"
#include "util/StaticString.hxx"
#include "time/DateTime.hpp"

NMEALogger::NMEALogger() noexcept {}
NMEALogger::~NMEALogger() noexcept = default;

inline void
NMEALogger::Start()
{
  if (file != nullptr)
    return;

  file = std::make_unique<FileOutputStream>(
      AllocatedPath::Build(MakeLocalPath("logs"),
      (DateTime::str_now("%Y%m%d-%H%M%S") + ".nmea").c_str()),
  FileOutputStream::Mode::APPEND_OR_CREATE);
}

static void
WriteLine(OutputStream &os, std::string_view text)
{
  os.Write(AsBytes(text));

  static constexpr char newline = '\n';
  os.Write(ReferenceAsBytes(newline));
}

void
NMEALogger::Log(const char *text) noexcept
{
  if (!enabled)
    return;

  const std::lock_guard lock{mutex};

  try {
    Start();
    WriteLine(*file, text);
  } catch (...) {
  }
}
