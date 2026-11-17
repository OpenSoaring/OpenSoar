// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Device/Features.hpp"
#include "thread/Mutex.hxx"
#include "ui/event/Notify.hpp"
#include "ui/event/Timer.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class MultipleDevices;
class EventLoop;

/**
 * Tells the devices at once when a serial port is plugged in or
 * pulled out, instead of leaving it to DeviceDescriptor::AutoReopen(),
 * which looks only every 30 seconds (and only once a minute after a
 * few failures).
 *
 * The platform reports the port from whatever thread it likes with
 * Post(); the devices are touched in the main thread only, a moment
 * later, because the port is usually not ready in the instant it is
 * announced (on Linux udev still has to set its permissions).
 *
 * Where the reports come from:
 *
 * - Linux (also OpenVario and Kobo): the kernel's device messages, read
 *   from a netlink socket; libudev is not needed
 * - Windows: WM_DEVICECHANGE, received by a hidden window of its own,
 *   because the SDL window does not hand that message on
 * - Android: UsbSerialHelper.java, through PostPortHotplug()
 *
 * Nothing breaks without a report: AutoReopen() still does its job.
 */
class PortHotplug {
public:
  /**
   * The platform's source of the reports; it lives as long as the
   * #PortHotplug that created it.
   */
  class Source {
  public:
    virtual ~Source() noexcept = default;
  };

private:
  struct Event {
    bool added;
    std::string name;
  };

  MultipleDevices &devices;

  Mutex mutex;
  std::vector<Event> queue;

  UI::Notify notify{[this]{ OnNotify(); }};
  UI::Timer timer{[this]{ OnTimer(); }};

  std::unique_ptr<Source> source;

public:
  PortHotplug(MultipleDevices &_devices, EventLoop &event_loop) noexcept;
  ~PortHotplug() noexcept;

  PortHotplug(const PortHotplug &) = delete;
  PortHotplug &operator=(const PortHotplug &) = delete;

  /**
   * Report a port that has appeared or is gone.  May be called from
   * any thread.
   *
   * @param name the name the platform gives the port: a device node
   * on Linux, "COM3" on Windows, the USB port id on Android
   */
  void Post(bool added, std::string_view name) noexcept;

private:
  void OnNotify() noexcept;
  void OnTimer() noexcept;
};

/**
 * Report a port to the #PortHotplug of the running program, if there
 * is one.  May be called from any thread.
 */
void
PostPortHotplug(bool added, std::string_view name) noexcept;

#ifdef HAVE_REMOTE_STICK

/**
 * Look again for the SteFly devices of the automatic slots, as at the
 * start, and bind the ones that are new: a RemoteStick plugged in
 * while the program runs works as a keyboard, but without the serial
 * connection it sends no settings and nothing reaches the log.
 *
 * A slot whose device is already bound to the port found keeps running
 * untouched; a slot bound to another port (the device came back with
 * another COM number) is moved to the new one.  A device that is gone
 * keeps its slot, so that the descriptor reopens it when it comes
 * back.
 *
 * @param occasion completes the log line "found on X ..."
 */
void
BindNewSteFlyDevices(MultipleDevices *devices,
                     const char *occasion) noexcept;

#endif
