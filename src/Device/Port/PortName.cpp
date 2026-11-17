// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "PortName.hpp"

unsigned
ParseComPortNumber(std::string_view name) noexcept
{
  if (name.starts_with("\\\\.\\"))
    name.remove_prefix(4);

  if (name.size() < 4 ||
      (name[0] != 'C' && name[0] != 'c') ||
      (name[1] != 'O' && name[1] != 'o') ||
      (name[2] != 'M' && name[2] != 'm'))
    return 0;

  name.remove_prefix(3);
  if (name.ends_with(':'))
    name.remove_suffix(1);

  if (name.empty() || name.size() > 4)
    return 0;

  unsigned number = 0;
  for (char ch : name) {
    if (ch < '0' || ch > '9')
      return 0;

    number = number * 10 + unsigned(ch - '0');
  }

  return number;
}

bool
ParseTtyUevent(std::string_view message, bool &added,
               std::string &device) noexcept
{
  /* the message is a list of strings, each ended by a null byte: a
     summary ("add@/devices/...") first, then "KEY=value" pairs */
  bool have_action = false, is_tty = false;
  std::string_view name;

  while (!message.empty()) {
    const auto end = message.find('\0');
    const std::string_view line = message.substr(0, end);
    message = end == message.npos
      ? std::string_view{}
      : message.substr(end + 1);

    if (line == "ACTION=add") {
      have_action = true;
      added = true;
    } else if (line == "ACTION=remove") {
      have_action = true;
      added = false;
    } else if (line == "SUBSYSTEM=tty")
      is_tty = true;
    else if (line.starts_with("DEVNAME="))
      name = line.substr(8);
  }

  if (!have_action || !is_tty || name.empty())
    return false;

  /* the kernel gives the name below /dev; udev's copies of the
     message carry the whole path */
  device = name.starts_with('/')
    ? std::string{name}
    : std::string{"/dev/"} + std::string{name};
  return true;
}
