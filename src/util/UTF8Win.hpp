// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once


#ifdef _WIN32
// #include <codecvt>
#include <locale>
#include <string>

std::string ToUTF8(const std::string &str,
                    const std::locale &loc = std::locale{});
std::string FromUTF8(const std::string &str,
                      const std::locale &loc = std::locale{});
#endif