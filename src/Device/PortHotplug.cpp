// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "PortHotplug.hpp"
#include "PortHotplugSource.hpp"
#include "MultipleDevices.hpp"
#include "Descriptor.hpp"
#include "Operation/Operation.hpp"
#include "LogFile.hpp"

#ifdef HAVE_REMOTE_STICK
#include "Device/Driver/SteFly/Discovery.hpp"
#include "Operation/MessageOperationEnvironment.hpp"
#include "Interface.hpp"
#endif

#include <chrono>

/**
 * How long a report waits before the devices hear of it.  A port is
 * announced before it can be used: on Linux udev sets its permissions
 * after the kernel has announced it, on Windows the driver is still
 * being started.
 */
static constexpr std::chrono::milliseconds SETTLE_TIME{1500};

/**
 * The instance that PostPortHotplug() reports to.  The mutex keeps it
 * alive while a platform thread is posting.
 */
static Mutex instance_mutex;
static PortHotplug *instance;

PortHotplug::PortHotplug(MultipleDevices &_devices,
                         EventLoop &event_loop) noexcept
  :devices(_devices),
   source(CreatePortHotplugSource(*this, event_loop))
{
  const std::lock_guard lock{instance_mutex};
  instance = this;
}

PortHotplug::~PortHotplug() noexcept
{
  {
    const std::lock_guard lock{instance_mutex};
    instance = nullptr;
  }

  /* no report can arrive after the source is gone */
  source.reset();

  notify.ClearNotification();
  timer.Cancel();
}

void
PortHotplug::Post(bool added, std::string_view name) noexcept
{
  {
    const std::lock_guard lock{mutex};
    queue.push_back({added, std::string{name}});
  }

  notify.SendNotification();
}

void
PostPortHotplug(bool added, std::string_view name) noexcept
{
  const std::lock_guard lock{instance_mutex};
  if (instance != nullptr)
    instance->Post(added, name);
}

void
PortHotplug::OnNotify() noexcept
{
  /* every further report starts the wait again, so that a hub with
     several devices is handled in one go */
  timer.Schedule(SETTLE_TIME);
}

void
PortHotplug::OnTimer() noexcept
{
  std::vector<Event> events;

  {
    const std::lock_guard lock{mutex};
    events.swap(queue);
  }

  /* this OperationEnvironment instance must be persistent, because
     DeviceDescriptor::Open() is asynchronous */
  static QuietOperationEnvironment env;

  bool any_added = false;

  for (const auto &event : events) {
    LogFmt("Serial port {}: {}", event.added ? "plugged in" : "pulled out",
           event.name);

    if (event.added) {
      any_added = true;
      devices.DetectedPort(event.name, env);
    } else
      devices.RemovedPort(event.name);
  }

#ifdef HAVE_REMOTE_STICK
  if (any_added)
    /* the slot of a SteFly device has no port until the device has
       been found on the bus once */
    BindNewSteFlyDevices(&devices, "after it was plugged in");
#else
  (void)any_added;
#endif
}

#ifdef HAVE_REMOTE_STICK

void
BindNewSteFlyDevices(MultipleDevices *devices,
                     const char *occasion) noexcept
{
  for (const auto &auto_slot : SteFly::AUTO_SLOTS) {
    DeviceConfig &config =
      CommonInterface::SetSystemSettings().devices[auto_slot.slot];

    DeviceConfig found = config;
    if (!SteFly::FindAutoSlotDevice(auto_slot, found))
      continue;

    if (config.UsesPort() && config.port_type == found.port_type &&
        config.path == found.path)
      /* already bound to this port */
      continue;

    LogFmt("SteFly {} found on {} {} - binding to device slot {}",
           auto_slot.driver_name, found.path.c_str(), occasion,
           auto_slot.slot);

    if (devices != nullptr && (*devices)[auto_slot.slot].IsOccupied())
      /* somebody is working with the slot; it is bound the next time
         somebody looks */
      continue;

    config = found;

    if (devices != nullptr) {
      DeviceDescriptor &descriptor = (*devices)[auto_slot.slot];
      descriptor.SetConfig(config);

      /* this OperationEnvironment instance must be persistent, because
         DeviceDescriptor::Open() is asynchronous */
      static MessageOperationEnvironment env;
      descriptor.Reopen(env);
    }
  }
}

#endif // HAVE_REMOTE_STICK
