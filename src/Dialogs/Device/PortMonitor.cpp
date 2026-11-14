// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "PortMonitor.hpp"
#include "Dialogs/Message.hpp"
#include "Dialogs/TextEntry.hpp"
#include "Look/Look.hpp"
#include "ui/control/TerminalWindow.hpp"
#include "Widget/WindowWidget.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Device/Descriptor.hpp"
#include "util/StaticFifoBuffer.hxx"
#include "util/StaticString.hxx"
#include "Language/Language.hpp"
#include "Operation/MessageOperationEnvironment.hpp"
#include "ui/event/DelayedNotify.hpp"
#include "thread/Mutex.hxx"
#include "UIGlobals.hpp"

#include <cstring>

/**
 * A bridge between DataHandler and TerminalWindow: copy all data
 * received from the Port to the TerminalWindow.
 */
class PortTerminalBridge final : public DataHandler {
  TerminalWindow &terminal;
  Mutex mutex;
  StaticFifoBuffer<std::byte, 1024> buffer;

  UI::DelayedNotify notify{
    std::chrono::milliseconds(100),
    [this]{ OnNotification(); },
  };

public:
  PortTerminalBridge(TerminalWindow &_terminal)
    :terminal(_terminal) {}
  ~PortTerminalBridge() {}

  bool DataReceived(std::span<const std::byte> s) noexcept {
    {
      const std::lock_guard lock{mutex};
      buffer.Shift();
      auto range = buffer.Write();
      const std::size_t nbytes = std::min(s.size(), range.size());
      std::copy_n(s.begin(), nbytes, range.begin());
      buffer.Append(nbytes);
    }

    notify.SendNotification();
    return true;
  }

private:
  void OnNotification() noexcept {
    while (true) {
      std::array<std::byte, 64> data;
      size_t length;

      {
        const std::lock_guard lock{mutex};
        auto range = buffer.Read();
        if (range.empty())
          break;

        length = std::min(data.size(), range.size());
        std::copy_n(range.begin(), length, data.begin());
        buffer.Consume(length);
      }

      terminal.Write((const char *)data.data(), length);
    }
  }
};

class PortMonitorWidget final : public WindowWidget {
  DeviceDescriptor &device;
  const TerminalLook &look;
  std::unique_ptr<PortTerminalBridge> bridge;

  Button *pause_button;
  bool paused;

public:
  PortMonitorWidget(DeviceDescriptor &_device,
                    const TerminalLook &_look) noexcept
    :device(_device), look(_look), paused(false) {}

  void CreateButtons(WidgetDialog &dialog);

  void Clear() {
    auto &terminal = (TerminalWindow &)GetWindow();
    terminal.Clear();
  }

  void Reconnect();
  void TogglePause();

  /**
   * Ask for one NMEA sentence and write it to the port, so that a
   * command can be tried on a device (a FLARM, a vario, a SteFly panel,
   * a GPS module) while its answer shows in the monitor.
   */
  void Send();

  /* virtual methods from class Widget */

  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override {
    WindowStyle style;
    style.Hide();

    auto w = std::make_unique<TerminalWindow>(look);
    w->Create(parent, rc, style);

    bridge = std::make_unique<PortTerminalBridge>(*w);
    device.SetMonitor(bridge.get());

    SetWindow(std::move(w));
  }

  void Unprepare() noexcept override {
    device.SetMonitor(nullptr);
  }
};

void
PortMonitorWidget::CreateButtons(WidgetDialog &dialog)
{
  dialog.AddButton(_("Clear"), [this](){ Clear(); });
  dialog.AddButton(_("Reconnect"), [this](){ Reconnect(); });
  pause_button = dialog.AddButton(_("Pause"), [this](){ TogglePause(); });

  /* any device with a driver speaks NMEA and may take a command,
     whether or not its driver sends settings by itself; FLARM is the
     most common example of one that does not */
  if (device.GetConfig().UsesDriver())
    dialog.AddButton(_("Send"), [this](){ Send(); });
}

void
PortMonitorWidget::Reconnect()
{
  if (device.IsBorrowed()) {
    ShowMessageBox(_("Device is occupied"), _("Reconnect"),
                   MB_OK | MB_ICONERROR);
    return;
  }

  /* this OperationEnvironment instance must be persistent, because
     DeviceDescriptor::Open() is asynchronous */
  static MessageOperationEnvironment env;
  device.Reopen(env);
}

void
PortMonitorWidget::TogglePause()
{
  paused = !paused;

  if (paused) {
    pause_button->SetCaption(_("Resume"));
    device.SetMonitor(nullptr);
  } else {
    pause_button->SetCaption(_("Pause"));
    device.SetMonitor(bridge.get());
  }
}

void
PortMonitorWidget::Send()
{
  /* static, so that the last sentence is offered again: a command is
     usually changed a little and sent once more */
  static StaticString<128> sentence;

  if (!TextEntryDialog(sentence, _("Send NMEA sentence")))
    return;

  /* WriteNMEA() adds the "$" and the checksum itself, so a sentence
     copied from a log or a manual with both is accepted as well */
  StaticString<128> body;
  body = sentence.c_str() + (sentence.c_str()[0] == '$');
  if (char *star = std::strchr(body.data(), '*'); star != nullptr)
    *star = '\0';

  if (body.empty())
    return;

  if (device.GetState() != PortState::READY) {
    ShowMessageBox(_("Device is not connected"), _("Send"),
                   MB_OK | MB_ICONERROR);
    return;
  }

  /* a write error is shown by the environment itself */
  MessageOperationEnvironment env;
  device.WriteNMEA(body.c_str(), env);
}

void
ShowPortMonitor(DeviceDescriptor &device)
{
  const Look &look = UIGlobals::GetLook();

  std::array<char, 64> buffer;
  StaticString<128> caption;
  caption.Format("%s: %s", _("Port monitor"),
                 device.GetConfig().GetPortName(buffer.data(), buffer.size()));

  TWidgetDialog<PortMonitorWidget>
    dialog(WidgetDialog::Full{}, UIGlobals::GetMainWindow(),
           look.dialog, caption);
  dialog.AddButton(_("Close"), mrOK);
  dialog.SetWidget(device, look.terminal);
  dialog.GetWidget().CreateButtons(dialog);

  dialog.ShowModal();
}
