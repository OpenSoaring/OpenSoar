// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ManageRemoteDialog.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Dialogs/Error.hpp"
#include "Form/DataField/Enum.hpp"
#include "Widget/RowFormWidget.hpp"
#include "UIGlobals.hpp"
#include "Language/Language.hpp"
#include "Operation/Cancelled.hpp"
#include "Operation/MessageOperationEnvironment.hpp"
#include "Operation/PopupOperationEnvironment.hpp"
#include "Device/Driver/SteFly/RemoteStick.hpp"

// Per-block wait timeout. Generous on purpose — the stick replies in
// a few milliseconds, but a busy serial link plus an Android USB
// stack can take noticeably longer.
static constexpr unsigned INFO_TIMEOUT_MS     = 800;
static constexpr unsigned SETTINGS_TIMEOUT_MS = 800;

static constexpr StaticEnumChoice stefly_layout_names[] = {
  { unsigned(StickRemoteControl::RemoteStickSettings::Layout::BASIC),    "Basic" },
  { unsigned(StickRemoteControl::RemoteStickSettings::Layout::ADVANCED), "Advanced" },
  { unsigned(StickRemoteControl::RemoteStickSettings::Layout::ANDROID),  "Android" },
  { unsigned(StickRemoteControl::RemoteStickSettings::Layout::STARTER),  "Starter-Key" },
  nullptr
};

class ManageRemoteWidget final : public RowFormWidget {
  enum Controls {
    LAYOUT,
  };

  StickRemoteControl &device;
  // SteFlyInfo lives on the SteFlyDevice base class — it's the same
  // shape for every SteFly device, so we use the base type name.
  SteFlyDevice::SteFlyInfo                info;
  StickRemoteControl::RemoteStickSettings settings;

public:
  ManageRemoteWidget(const DialogLook &look,
                     StickRemoteControl &_device) noexcept
    :RowFormWidget(look), device(_device) {}

  /* virtual methods from Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;
};

void
ManageRemoteWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                            [[maybe_unused]] const PixelRect &rc) noexcept
{
  PopupOperationEnvironment env;

  // --- query the stick: info first, settings next ------------------
  // Info responses arrive on the "$PSRCI" talker, settings on
  // "$PSRCS"; each block ends with its own "Ready" sentence, so the
  // two waits never trip over each other. We do not fail the dialog
  // if the stick is silent — we just don't populate the read-only
  // fields. The user can still hit Reboot or change the layout.
  try {
    device.RequestInfo(env);
    device.WaitForInfo(INFO_TIMEOUT_MS);
    info = device.GetInfo();
  } catch (...) {
    // Swallow — Save/Reboot will surface errors when they actually hit.
  }

  try {
    device.RequestSettings(env);
    device.WaitForSettings(SETTINGS_TIMEOUT_MS);
    settings = device.GetSettings();
  } catch (...) {
  }

  // --- read-only info ----------------------------------------------
  if (info.available) {
    if (!info.version.empty())
      AddReadOnly(_("Firmware version"), nullptr, info.version.c_str());
    if (!info.file_name.empty())
      AddReadOnly(_("Config file"), nullptr, info.file_name.c_str());
    if (!info.serial_number.empty())
      AddReadOnly(_("Serial number"), nullptr, info.serial_number.c_str());
  }

  // --- editable settings -------------------------------------------
  AddEnum(_("Key Configuration"),
          _("Default Key Configuration"),
          stefly_layout_names,
          unsigned(settings.layout));

  // --- one-shot actions --------------------------------------------
  AddButton(_("Reboot"), [this]() {
    try {
      MessageOperationEnvironment env;
      device.Restart(env);
    } catch (...) {
      ShowError(std::current_exception(), _("Error"));
    }
  });
}

bool
ManageRemoteWidget::Save(bool &changed) noexcept
{
  PopupOperationEnvironment env;
  auto new_settings = settings;

  unsigned layout_value = unsigned(new_settings.layout);
  changed |= SaveValueEnum(LAYOUT, layout_value);
  new_settings.layout =
    StickRemoteControl::RemoteStickSettings::Layout(layout_value);

  try {
    device.WriteDeviceSettings(new_settings, env);
  } catch (OperationCancelled) {
    return false;
  } catch (...) {
    ShowError(std::current_exception(), "SteFly Remote Stick");
    return false;
  }

  return true;
}

void
ManageRemoteDialog(Device &device)
{
  WidgetDialog dialog(WidgetDialog::Auto{},
                      UIGlobals::GetMainWindow(),
                      UIGlobals::GetDialogLook(),
                      "SteFly Remote",
                      new ManageRemoteWidget(UIGlobals::GetDialogLook(),
                                             static_cast<StickRemoteControl &>(device)));
  dialog.AddButton(_("Close"), mrCancel);
  dialog.ShowModal();
}
