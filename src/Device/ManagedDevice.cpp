// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ManagedDevice.hpp"
#include "Device/Util/NMEAWriter.hpp"

#include <cassert>
#include <cstdio>

void
ManagedDevice::SendNMEA(const char *sentence, OperationEnvironment &env)
{
  assert(sentence != nullptr);
  PortWriteNMEA(port, sentence, env);
}

void
ManagedDevice::WriteDeviceSetting(const char *write_talker,
                                  char direction,
                                  const char *name,
                                  std::string_view value,
                                  OperationEnvironment &env)
{
  assert(write_talker != nullptr);
  assert(name != nullptr);
  assert(direction == 'R' || direction == 'S' ||
         direction == 'A' || direction == 'I');

  char buffer[128];
  const int written = std::snprintf(buffer, sizeof(buffer),
                                    "%s,%c,%s,%.*s",
                                    write_talker, direction, name,
                                    int(value.size()), value.data());
  if (written < 0 || written >= int(sizeof(buffer)))
    return; // would be truncated, drop the sentence

  PortWriteNMEA(port, buffer, env);
}

void
ManagedDevice::WriteDeviceSetting(const char *write_talker,
                                  const char *name,
                                  std::string_view value,
                                  OperationEnvironment &env)
{
  assert(write_talker != nullptr);
  assert(name != nullptr);

  char buffer[128];
  const int written = std::snprintf(buffer, sizeof(buffer),
                                    "%s,%s,%.*s",
                                    write_talker, name,
                                    int(value.size()), value.data());
  if (written < 0 || written >= int(sizeof(buffer)))
    return; // would be truncated, drop the sentence

  PortWriteNMEA(port, buffer, env);
}
