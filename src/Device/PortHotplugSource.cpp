// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "PortHotplugSource.hpp"
#include "Port/PortName.hpp"
#include "LogFile.hpp"

#if defined(__linux__) && !defined(ANDROID)

/* Linux: the kernel announces every device that appears or goes on a
   netlink socket.  udev reads the same messages; reading them here
   needs no libudev, which the OpenVario and the Kobo do not have. */

#include "event/SocketEvent.hxx"
#include "event/Call.hxx"
#include "net/SocketDescriptor.hxx"

#include <sys/socket.h>
#include <linux/netlink.h>

class LinuxPortHotplugSource final : public PortHotplug::Source {
  PortHotplug &hotplug;
  SocketEvent event;

public:
  LinuxPortHotplugSource(PortHotplug &_hotplug, EventLoop &event_loop,
                         SocketDescriptor fd) noexcept
    :hotplug(_hotplug),
     event(event_loop, BIND_THIS_METHOD(OnSocketReady), fd)
  {
    /* the socket may only be registered in the thread of the loop */
    BlockingCall(event_loop, [this]{ event.ScheduleRead(); });
  }

  ~LinuxPortHotplugSource() noexcept override {
    BlockingCall(event.GetEventLoop(), [this]{ event.Close(); });
  }

private:
  void OnSocketReady(unsigned events) noexcept;
};

void
LinuxPortHotplugSource::OnSocketReady([[maybe_unused]] unsigned events) noexcept
{
  char buffer[8192];
  struct sockaddr_nl sender{};
  socklen_t sender_size = sizeof(sender);

  const ssize_t nbytes = recvfrom(event.GetSocket().Get(), buffer,
                                  sizeof(buffer), MSG_DONTWAIT,
                                  (struct sockaddr *)&sender,
                                  &sender_size);
  if (nbytes <= 0)
    return;

  /* only the kernel (port id 0) is believed; any local process can
     send a message to this socket */
  if (sender_size < sizeof(sender) || sender.nl_pid != 0)
    return;

  bool added;
  std::string device;
  if (ParseTtyUevent({buffer, std::size_t(nbytes)}, added, device))
    hotplug.Post(added, device);
}

std::unique_ptr<PortHotplug::Source>
CreatePortHotplugSource(PortHotplug &hotplug,
                        EventLoop &event_loop) noexcept
{
  SocketDescriptor fd;
  if (!fd.CreateNonBlock(AF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT)) {
    LogString("Serial port hotplug: no netlink socket");
    return nullptr;
  }

  struct sockaddr_nl address{};
  address.nl_family = AF_NETLINK;
  /* group 1 carries the messages of the kernel itself */
  address.nl_groups = 1;

  if (bind(fd.Get(), (const struct sockaddr *)&address,
           sizeof(address)) < 0) {
    /* e.g. not permitted inside a container */
    LogString("Serial port hotplug: cannot listen to the kernel");
    fd.Close();
    return nullptr;
  }

  return std::make_unique<LinuxPortHotplugSource>(hotplug, event_loop, fd);
}

#elif defined(_WIN32)

/* Windows announces a serial port with WM_DEVICECHANGE to every
   top-level window.  The window of the program belongs to SDL, which
   does not hand that message on, so a window of our own listens; it
   is never shown.  Its messages are dispatched by the message loop of
   the thread that created it, which is the main thread. */

#include <windows.h>
#include <dbt.h>

class WindowsPortHotplugSource final : public PortHotplug::Source {
  static constexpr const char *CLASS_NAME = "OpenSoarPortHotplug";

  PortHotplug &hotplug;
  HWND window = nullptr;

public:
  explicit WindowsPortHotplugSource(PortHotplug &_hotplug) noexcept;
  ~WindowsPortHotplugSource() noexcept override;

  bool IsDefined() const noexcept {
    return window != nullptr;
  }

private:
  void OnDeviceChange(WPARAM wParam, LPARAM lParam) noexcept;

  static LRESULT CALLBACK WindowProcedure(HWND hWnd, UINT message,
                                          WPARAM wParam,
                                          LPARAM lParam) noexcept;
};

WindowsPortHotplugSource::WindowsPortHotplugSource(PortHotplug &_hotplug) noexcept
  :hotplug(_hotplug)
{
  const HINSTANCE instance = GetModuleHandle(nullptr);

  WNDCLASSA window_class{};
  window_class.lpfnWndProc = WindowProcedure;
  window_class.hInstance = instance;
  window_class.lpszClassName = CLASS_NAME;
  RegisterClassA(&window_class);

  /* a top-level window: a message-only window (HWND_MESSAGE) does not
     get the broadcast */
  window = CreateWindowExA(0, CLASS_NAME, CLASS_NAME, WS_POPUP,
                           0, 0, 0, 0, nullptr, nullptr, instance,
                           nullptr);
  if (window != nullptr)
    SetWindowLongPtrA(window, GWLP_USERDATA, (LONG_PTR)this);
}

WindowsPortHotplugSource::~WindowsPortHotplugSource() noexcept
{
  if (window != nullptr)
    DestroyWindow(window);

  UnregisterClassA(CLASS_NAME, GetModuleHandle(nullptr));
}

void
WindowsPortHotplugSource::OnDeviceChange(WPARAM wParam,
                                         LPARAM lParam) noexcept
{
  if (lParam == 0 ||
      (wParam != DBT_DEVICEARRIVAL && wParam != DBT_DEVICEREMOVECOMPLETE))
    return;

  const auto *header = (const DEV_BROADCAST_HDR *)lParam;
  if (header->dbch_devicetype != DBT_DEVTYP_PORT)
    return;

  /* the ANSI structure, because the window class is an ANSI one */
  const auto *port = (const DEV_BROADCAST_PORT_A *)lParam;

  /* parallel ports ("LPT1") are announced the same way */
  if (ParseComPortNumber(port->dbcp_name) == 0)
    return;

  hotplug.Post(wParam == DBT_DEVICEARRIVAL, port->dbcp_name);
}

LRESULT CALLBACK
WindowsPortHotplugSource::WindowProcedure(HWND hWnd, UINT message,
                                          WPARAM wParam,
                                          LPARAM lParam) noexcept
{
  if (message == WM_DEVICECHANGE) {
    auto *source = (WindowsPortHotplugSource *)
      GetWindowLongPtrA(hWnd, GWLP_USERDATA);
    if (source != nullptr)
      source->OnDeviceChange(wParam, lParam);
  }

  return DefWindowProcA(hWnd, message, wParam, lParam);
}

std::unique_ptr<PortHotplug::Source>
CreatePortHotplugSource(PortHotplug &hotplug,
                        [[maybe_unused]] EventLoop &event_loop) noexcept
{
  auto source = std::make_unique<WindowsPortHotplugSource>(hotplug);
  if (!source->IsDefined()) {
    LogString("Serial port hotplug: cannot create the window that listens");
    return nullptr;
  }

  return source;
}

#else

/* Android reports through PostPortHotplug() from UsbSerialHelper.java;
   the other platforms have no source */

std::unique_ptr<PortHotplug::Source>
CreatePortHotplugSource([[maybe_unused]] PortHotplug &hotplug,
                        [[maybe_unused]] EventLoop &event_loop) noexcept
{
  return nullptr;
}

#endif
