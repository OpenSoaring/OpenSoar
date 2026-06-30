// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ManageRemoteDialog.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Dialogs/JobDialog.hpp"
#include "Dialogs/Message.hpp"
#include "Dialogs/Error.hpp"
#include "Form/DataField/Enum.hpp"
#include "Widget/RowFormWidget.hpp"
#include "UIGlobals.hpp"
#include "Language/Language.hpp"
#include "Operation/Cancelled.hpp"
#include "Operation/MessageOperationEnvironment.hpp"
#include "Operation/PopupOperationEnvironment.hpp"
#include "Operation/Operation.hpp"
#include "Device/Driver/SteFly/RemoteStick.hpp"
#include "Device/Descriptor.hpp"
#include "Device/Port/State.hpp"
#include "Job/Job.hpp"
#include "LogFile.hpp"

#include <chrono>
#include <thread>

// Per-block wait timeout for the post-open info / settings query.
// Generous on purpose — the stick replies in a few milliseconds, but
// a busy serial link plus an Android USB stack can take noticeably
// longer.
static constexpr unsigned INFO_TIMEOUT_MS     = 800;
static constexpr unsigned SETTINGS_TIMEOUT_MS = 800;

// Reboot wait budget — the wait job needs to see BOTH the USB
// disconnect and the subsequent reconnect within this window. On
// Windows the disconnect edge alone can take 5-7 seconds after the
// firmware acknowledges the reboot command (USB stack timing), so
// be generous.
static constexpr auto REBOOT_TOTAL_TIMEOUT = std::chrono::seconds(20);
static constexpr auto REBOOT_POLL_INTERVAL = std::chrono::milliseconds(200);

// PortMonitor's Reopen() runs on the main thread; right after it
// finishes async work may still be settling. Retry the Borrow many
// times before giving up — the worst-case Reopen on Android USB can
// take a couple of seconds to fully settle.
static constexpr unsigned REBOOT_BORROW_RETRIES  = 50;
static constexpr auto REBOOT_BORROW_INTERVAL     = std::chrono::milliseconds(100);

// The "User" mode is read-only — the firmware reports it when the
// active key map doesn't match any preset, but it can never be set
// from this dialog. We therefore keep two near-identical layout
// lists: the regular one (no USER row, that's what the dropdown
// shows in 99% of opens) and an extended one that includes USER
// purely so the dropdown can DISPLAY a USER state when the stick is
// actually in it. Picking USER would still be a no-op (see Save()
// and StickRemoteControl::WriteDeviceSettings).
static constexpr StaticEnumChoice stefly_layout_names[] = {
  { unsigned(StickRemoteControl::RemoteStickSettings::Layout::BASIC),    "Basic" },
  { unsigned(StickRemoteControl::RemoteStickSettings::Layout::ADVANCED), "Advanced" },
  { unsigned(StickRemoteControl::RemoteStickSettings::Layout::ANDROID0), "Android" },
  { unsigned(StickRemoteControl::RemoteStickSettings::Layout::ANDROID1), "Android (/w App switch)" },
  { unsigned(StickRemoteControl::RemoteStickSettings::Layout::STARTER0), "Starter-Key" },
  { unsigned(StickRemoteControl::RemoteStickSettings::Layout::STARTER1), "Starter-Key (/w App switch)" },
  nullptr
};

static constexpr StaticEnumChoice stefly_layout_names_with_user[] = {
  { unsigned(StickRemoteControl::RemoteStickSettings::Layout::BASIC),    "Basic" },
  { unsigned(StickRemoteControl::RemoteStickSettings::Layout::ADVANCED), "Advanced" },
  { unsigned(StickRemoteControl::RemoteStickSettings::Layout::ANDROID0), "Android" },
  { unsigned(StickRemoteControl::RemoteStickSettings::Layout::ANDROID1), "Android (/w App switch)" },
  { unsigned(StickRemoteControl::RemoteStickSettings::Layout::STARTER0), "Starter-Key" },
  { unsigned(StickRemoteControl::RemoteStickSettings::Layout::STARTER1), "Starter-Key (/w App switch)" },
  { unsigned(StickRemoteControl::RemoteStickSettings::Layout::USER),     "User (custom keys)" },
  nullptr
};

namespace {

/**
 * Modal-friendly job that polls the descriptor's port state until it
 * comes back to PortState::READY or the timeout / cancellation fires.
 *
 * Used by the Reboot button: after the stick has reset its USB stack
 * the descriptor's PortMonitor closes and re-opens the port, and we
 * wait here for the "ready again" edge before re-borrowing.
 */
class WaitForReconnectJob final : public Job {
  DeviceDescriptor &descriptor;
  std::chrono::milliseconds total_timeout;
  bool success = false;

public:
  WaitForReconnectJob(DeviceDescriptor &_descriptor,
                      std::chrono::milliseconds _timeout) noexcept
    :descriptor(_descriptor), total_timeout(_timeout) {}

  bool WasSuccessful() const noexcept { return success; }

  void Run(OperationEnvironment &env) override {
    using Clock = std::chrono::steady_clock;

    LogString("RebootStick: wait job started");

    // Single combined wait: we must observe a real state TRANSITION
    // (disconnect followed by reconnect) before declaring success.
    // Just polling for "state == READY" would race straight through
    // the "still ready" window — on Windows the USB stack can take
    // 5–7 seconds to drop the device after the firmware ack'd the
    // Reboot command, and during that window descriptor.GetState()
    // is still READY even though the device is about to disappear.
    // Re-borrowing in that window means the PortMonitor's later
    // Close() trips assert(!IsBorrowed()) — which is the bug this
    // logic exists to prevent.
    env.SetText(_("Waiting for stick to reboot…"));
    const auto deadline = Clock::now() + total_timeout;
    bool saw_disconnect = false;

    while (Clock::now() < deadline) {
      if (env.IsCancelled()) {
        LogString("RebootStick: cancelled");
        return;
      }

      const auto state = descriptor.GetState();
      if (state != PortState::READY) {
        if (!saw_disconnect) {
          saw_disconnect = true;
          env.SetText(_("Waiting for stick to reconnect…"));
          LogString("RebootStick: disconnect seen");
        }
      } else if (saw_disconnect) {
        // back to READY after we had observed the disconnect
        success = true;
        LogString("RebootStick: reconnect seen, state READY");
        return;
      }

      env.Sleep(REBOOT_POLL_INTERVAL);
    }

    LogFmt("RebootStick: wait timed out (saw_disconnect={})",
           saw_disconnect ? "yes" : "no");
  }
};

} // namespace

class ManageRemoteWidget final : public RowFormWidget {
  enum Controls {
    LAYOUT,
  };

  DeviceDescriptor &descriptor;
  // Pointer — NOT a reference — because the underlying Device* is
  // recreated by DeviceDescriptor::Reopen() when the stick disconnects
  // (firmware reboot, USB unplug, etc.). After every Reopen we re-fetch
  // the fresh pointer via descriptor.GetDevice(); a reference would be
  // a use-after-free as soon as the first reboot finishes.
  StickRemoteControl *device = nullptr;

  // SteFlyInfo lives on the SteFlyDevice base class — same shape for
  // every SteFly device, so we use the base type name.
  SteFlyDevice::SteFlyInfo                info;
  StickRemoteControl::RemoteStickSettings settings;

public:
  ManageRemoteWidget(const DialogLook &look,
                     DeviceDescriptor &_descriptor,
                     StickRemoteControl &_device) noexcept
    :RowFormWidget(look), descriptor(_descriptor), device(&_device) {}

private:
  /** Re-fetch the live Device* from the descriptor. Must be called
   *  after every Reopen(); returns false (and clears `device`) when
   *  the descriptor currently has no device. */
  bool RefreshDevicePointer() noexcept {
    device = static_cast<StickRemoteControl *>(descriptor.GetDevice());
    return device != nullptr;
  }

public:

  /* virtual methods from Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;

private:
  /**
   * Run the post-open info + settings queries; updates the cached
   * `info` and `settings` members. Called from Prepare() (initial
   * population) and after a successful Reboot (so Save() diffs
   * against the post-reboot state).
   */
  void RefreshFromDevice() noexcept;

  /**
   * Send a reboot to the stick and wait for the USB to come back.
   * Releases the descriptor borrow during the disconnect window so
   * the PortMonitor can Close / Reopen freely, then re-borrows.
   *
   * @return true if the device is once again borrowed and ready,
   *         false if reboot, reconnect or re-borrow failed
   *         (caller should close the manage dialog in that case).
   */
  bool RebootStick() noexcept;
};

void
ManageRemoteWidget::RefreshFromDevice() noexcept
{
  PopupOperationEnvironment env;

  if (device == nullptr)
    return;  // device gone (e.g. failed reboot) — nothing to refresh

  // Info responses arrive on the "$PSRCI" talker, settings on
  // "$PSRCS"; each block ends with its own "Ready" sentence, so the
  // two waits never trip over each other. We do not fail the dialog
  // if the stick is silent — we just don't populate the cached info.
  try {
    device->RequestInfo(env);
    device->WaitForInfo(INFO_TIMEOUT_MS);
    info = device->GetInfo();
  } catch (...) {
    // Swallow — Save/Reboot will surface errors when they actually hit.
  }

  try {
    device->RequestSettings(env);
    device->WaitForSettings(SETTINGS_TIMEOUT_MS);
    settings = device->GetSettings();
  } catch (...) {
  }
}

bool
ManageRemoteWidget::RebootStick() noexcept
{
  if (device == nullptr) {
    ShowMessageBox(_("No active device."),
                   _("SteFly Remote Stick"),
                   MB_OK | MB_ICONWARNING);
    return false;
  }

  // 1. Send the reboot sentence. Still borrowed — the stick is about
  //    to disappear from USB, so writing is the last thing we get to
  //    do on this port instance. After this call our `device` pointer
  //    is conceptually a walking-dead — Close() will delete the
  //    underlying Device object as soon as the USB disconnect fires.
  LogString("RebootStick: sending reboot sentence");
  try {
    MessageOperationEnvironment env;
    device->Restart(env);
  } catch (...) {
    ShowError(std::current_exception(), _("Reboot"));
    return false;
  }
  // Be defensive: stop using the soon-stale pointer until we have
  // re-fetched a fresh one from the descriptor after the Reopen.
  device = nullptr;

  // 2. Release the borrow so the PortMonitor's Close / Reopen path
  //    can run without tripping the assert(!IsBorrowed()) inside
  //    DeviceDescriptor::Close().
  LogString("RebootStick: returning borrow");
  descriptor.Return();

  // 3. Modal wait until the port shows up as READY again. JobDialog
  //    runs the job on a worker thread and pops a small dialog with
  //    a Cancel button; we poll descriptor.GetState() every 200 ms.
  WaitForReconnectJob job{descriptor, REBOOT_TOTAL_TIMEOUT};
  const bool job_finished =
    JobDialog(UIGlobals::GetMainWindow(),
              UIGlobals::GetDialogLook(),
              _("Rebooting Remote Stick"),
              job,
              true /* cancellable */);

  if (!job_finished || !job.WasSuccessful()) {
    ShowMessageBox(_("Device did not reconnect after reboot."),
                   _("SteFly Remote Stick"),
                   MB_OK | MB_ICONWARNING);
    // Leave the descriptor unborrowed; the outer ScopeReturnDevice
    // is now a no-op (see DeviceDescriptor.hpp::ScopeReturnDevice).
    return false;
  }

  // 4. Re-borrow now that the port is back. CanBorrow() also checks
  //    device != nullptr and !IsOccupied(). The PortMonitor's
  //    Reopen() may still have a tiny tail of async work going even
  //    after GetState() flips to READY, so retry many times.
  bool reborrowed = false;
  for (unsigned i = 0; i < REBOOT_BORROW_RETRIES; ++i) {
    if (descriptor.Borrow()) {
      reborrowed = true;
      LogFmt("RebootStick: reborrow ok after {} attempts", i + 1);
      break;
    }
    if (i == 0 || i + 1 == REBOOT_BORROW_RETRIES) {
      // Log first + last attempt only — middle attempts are noise.
      LogFmt("RebootStick: borrow attempt {} failed "
             "(state={}, has_device={})",
             i + 1,
             int(descriptor.GetState()),
             descriptor.GetDevice() != nullptr);
    }
    std::this_thread::sleep_for(REBOOT_BORROW_INTERVAL);
  }
  if (!reborrowed) {
    ShowMessageBox(_("Could not re-acquire device after reboot."),
                   _("SteFly Remote Stick"),
                   MB_OK | MB_ICONWARNING);
    return false;
  }

  // 5. Pick up the fresh Device* — the previous one was deleted by
  //    DeviceDescriptor::Close() during the USB disconnect, and
  //    Open() has just created a new instance.
  if (!RefreshDevicePointer()) {
    ShowMessageBox(_("Reopen left no active device."),
                   _("SteFly Remote Stick"),
                   MB_OK | MB_ICONWARNING);
    descriptor.Return();   // keep the borrow state consistent
    return false;
  }

  // 6. Refresh cached info / settings so Save() diffs against the
  //    real post-reboot state, not the pre-reboot snapshot.
  RefreshFromDevice();
  return true;
}

void
ManageRemoteWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                            [[maybe_unused]] const PixelRect &rc) noexcept
{
  RefreshFromDevice();

  // --- editable settings (MUST come first so the Controls enum
  //     indices line up — SaveValueEnum below uses LAYOUT == 0).
  //
  // Show the "User" entry only when the stick is actually in the
  // USER state right now (i.e. the EEPROM holds a custom key map).
  // Otherwise it stays hidden so the user is not tempted to pick a
  // value that we can't actually write to the device.
  const auto *layout_choices =
    settings.layout == StickRemoteControl::RemoteStickSettings::Layout::USER
      ? stefly_layout_names_with_user
      : stefly_layout_names;
  AddEnum(_("Key Configuration"),
          _("Default Key Configuration"),
          layout_choices,
          unsigned(settings.layout));

  // --- read-only info ----------------------------------------------
  if (info.available) {
    if (!info.name.empty())
      AddReadOnly(_("Device name"), nullptr, info.name.c_str());
    if (!info.version.empty())
      AddReadOnly(_("Firmware version"), nullptr, info.version.c_str());
    if (!info.file_name.empty())
      AddReadOnly(_("Config file"), nullptr, info.file_name.c_str());
    if (!info.serial_number.empty())
      AddReadOnly(_("Serial number"), nullptr, info.serial_number.c_str());
  }

  // --- one-shot actions --------------------------------------------
  // The Reboot lambda handles the full borrow / wait / re-borrow
  // dance so the manage dialog stays open across the reboot. On a
  // failure RebootStick() shows its own message box and leaves the
  // descriptor unborrowed — the dialog stays up and the user can
  // close it manually via Cancel. (The Reboot button is guarded by
  // a null-check in RebootStick(), so a second click after a failed
  // first attempt is safe.)
  AddButton(_("Reboot Remote Stick"), [this]() {
    RebootStick();
  });
}

bool
ManageRemoteWidget::Save(bool &changed) noexcept
{
  PopupOperationEnvironment env;
  auto new_settings = settings;

  unsigned layout_value = unsigned(new_settings.layout);
  changed |= SaveValueEnum(LAYOUT, layout_value);
  const auto chosen =
    StickRemoteControl::RemoteStickSettings::Layout(layout_value);

  // USER is a read-only state the firmware reports when the active
  // key map doesn't match any preset — it "emerges" from custom key
  // assignments and cannot be selected explicitly. If the user
  // picked USER from the dropdown we silently keep the previous
  // layout so the dialog does not write a meaningless "Layout=255"
  // sentence to the stick.
  if (chosen != StickRemoteControl::RemoteStickSettings::Layout::USER)
    new_settings.layout = chosen;

  if (device == nullptr)
    return false;  // device gone after a failed reboot; nothing to write

  try {
    device->WriteDeviceSettings(new_settings, env);
  } catch (OperationCancelled) {
    return false;
  } catch (...) {
    ShowError(std::current_exception(), "SteFly Remote Stick");
    return false;
  }

  return true;
}

void
ManageRemoteDialog(DeviceDescriptor &descriptor, Device &device)
{
  WidgetDialog dialog(WidgetDialog::Auto{},
                      UIGlobals::GetMainWindow(),
                      UIGlobals::GetDialogLook(),
                      "SteFly Remote",
                      new ManageRemoteWidget(UIGlobals::GetDialogLook(),
                                             descriptor,
                                             static_cast<StickRemoteControl &>(device)));
  // OK triggers the widget's Save() — that's where the modified
  // layout (and any future setting) is actually written to the stick
  // via device.WriteDeviceSettings(). Without this, closing the
  // dialog discards the user's selection.
  dialog.AddButton(_("OK"), mrOK);
  dialog.AddButton(_("Cancel"), mrCancel);
  dialog.ShowModal();
}
