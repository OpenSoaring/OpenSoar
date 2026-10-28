// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <string_view>

/**
 * The number of a Windows serial port given by its name, or 0 if the
 * name is not one.
 *
 * OpenSoar names a serial port "COMn", as the Device Manager does, and
 * only the call that opens it uses the device path "\\.\COMn" (see
 * SerialPort::Open()).  Older versions and XCSoar wrote "COMn:" (a
 * leftover of Windows CE) for COM1 to COM9 and "\\.\COMn" from COM10
 * on, and those spellings are still found in existing device settings,
 * so all three are understood here.
 */
[[gnu::pure]]
constexpr unsigned
ParseComPortNumber(std::string_view name) noexcept
{
  if (name.starts_with("\\\\.\\"))
    name.remove_prefix(4);

  if (!name.starts_with("COM"))
    return 0;
  name.remove_prefix(3);

  if (name.ends_with(':'))
    name.remove_suffix(1);

  /* Windows numbers its ports up to COM255 */
  if (name.empty() || name.size() > 3)
    return 0;

  unsigned number = 0;
  for (const char ch : name) {
    if (ch < '0' || ch > '9')
      return 0;
    number = number * 10 + unsigned(ch - '0');
  }

  return number;
}
