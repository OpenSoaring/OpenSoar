// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ManageRemoteDialog.hpp"
// #include "FLARM/ConfigWidget.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Dialogs/ComboPicker.hpp"
#include "Dialogs/Error.hpp"
#include "Form/DataField/ComboList.hpp"
#include "Widget/RowFormWidget.hpp"
#include "UIGlobals.hpp"
#include "Language/Language.hpp"
#include "Operation/MessageOperationEnvironment.hpp"
#include "Operation/PopupOperationEnvironment.hpp"
#include "Device/Driver/RemoteStick.hpp"
// #include "Device/Driver/FLARM/Device.hpp"
// #include "FLARM/Version.hpp"
// #include "FLARM/Hardware.hpp"
// #include "FLARM/State.hpp"

#include <string>


class ManageRemoteWidget final
  : public RowFormWidget {
  enum Controls {
    Setup,
    Reboot,
  };

  StickRemoteControl &device;
//  const FlarmVersion version;
//  FlarmHardware hardware;
//  const FlarmState state;

public:
  ManageRemoteWidget(const DialogLook &look, StickRemoteControl &_device)
                    // const FlarmVersion &version,
                    // FlarmHardware &hardware,
                    // const FlarmState &_state)
    :RowFormWidget(look), device(_device) {}
//     version(version), hardware(hardware), state(_state) {}

  /* virtual methods from Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
};

static const char *const stefly_config_names[] = {
  "DEVTYPE",
  "CAP",
  "RADIOID",
  NULL
};

void
ManageRemoteWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                           [[maybe_unused]] const PixelRect &rc) noexcept
{
  PopupOperationEnvironment env;
#if 0
  if(device.RequestAllSettings(flarm_config_names, env)) {
    if (const auto x = device.GetSetting("DEVTYPE"))
      hardware.device_type = *x;

    if (const auto x = device.GetSetting("CAP"))
      hardware.capabilities = *x;

    if (const auto x = device.GetSetting("RADIOID")) {
      if (const char *id = strchr(x->c_str(), ',')) {
        hardware.radio_id = FlarmId::Parse(id + 1, nullptr);
      }
    }

    hardware.available.Update(TimeStamp{FloatDuration{1}});
  }

  if (hardware.available) {
    StaticString<64> buffer;

    if (!hardware.device_type.empty()) {
      buffer.clear();
      buffer.UnsafeAppendASCII(hardware.device_type.c_str());
      AddReadOnly(_("Hardware type"), NULL, buffer.c_str());
    }

    if (hardware.radio_id.IsDefined()) {
      char tmp_id[10];    
      buffer.clear();
      buffer.UnsafeAppendASCII(hardware.radio_id.Format(tmp_id));
      AddReadOnly(_("Flarm ID"), NULL, buffer.c_str());
    }
  }

  if (version.available) {
    StaticString<64> buffer;

    if (!version.hardware_version.empty()) {
      buffer.clear();
      buffer.UnsafeAppendASCII(version.hardware_version.c_str());
      AddReadOnly(_("Hardware version"), NULL, buffer.c_str());
    }

    if (!version.software_version.empty()) {
      buffer.clear();
      buffer.UnsafeAppendASCII(version.software_version.c_str());
      AddReadOnly(_("Firmware version"), NULL, buffer.c_str());
    }

    if (!version.obstacle_version.empty()) {
      buffer.clear();
      buffer.UnsafeAppendASCII(version.obstacle_version.c_str());
      AddReadOnly(_("Obstacle database"), NULL, buffer.c_str());
    }
  }

  if (state.available) {
    AddReadOnly(_("Flight state"), nullptr,
                state.flight == FlarmState::Flight::IN_FLIGHT
                ? _("In flight") : _("On ground"));

    const char *recorder_text;
    switch (state.recorder) {
    case FlarmState::Recorder::RECORDING:
      recorder_text = _("Recording");
      break;
    case FlarmState::Recorder::BARO_ONLY:
      recorder_text = _("Baro only");
      break;
    default:
      recorder_text = _("Off");
      break;
    }
    AddReadOnly(_("IGC recorder"), nullptr, recorder_text);
  }

  AddButton(_("Setup"), [this](){
    FLARMConfigWidget widget(GetLook(), device, hardware);
    DefaultWidgetDialog(UIGlobals::GetMainWindow(), GetLook(),
                        "FLARM", widget);
  });
#endif

  AddButton(_("Reboot"), [this](){
    // device.NMEA
#if 1
    try {
      MessageOperationEnvironment env;
      device.Restart(env);
      // Destroy();  // ??    } catch (OperationCancelled) {
    } catch (...) {
      ShowError(std::current_exception(), _("Error"));
    }
#endif
  });
#if 0
  AddButton(_("Simulation"), [this](){
    ComboList list;
    const auto sim1_label = std::to_string(1) + " - " + _("FLARM collision");
    list.Append(1, "1",
                sim1_label.c_str(),
                _("FLARM aircraft on collision course, "
                  "all alarm levels. 30s."));
    const auto sim2_label = std::to_string(2) + " - " + _("ADS-B collision");
    list.Append(2, "2",
                sim2_label.c_str(),
                _("ADS-B aircraft on collision course, "
                  "all alarm levels. 30s."));
    const auto sim3_label = std::to_string(3) + " - " + _("Mode-S collision");
    list.Append(3, "3",
                sim3_label.c_str(),
                _("Non-directional Mode-S aircraft on "
                  "collision course. 30s."));
    const auto sim4_label = std::to_string(4) + " - " + _("Obstacle");
    list.Append(4, "4",
                sim4_label.c_str(),
                _("Fixed obstacle from database, "
                  "all alarm levels. 30s."));
    const auto sim5_label = std::to_string(5) + " - " + _("Alert Zone");
    list.Append(5, "5",
                sim5_label.c_str(),
                _("Alert Zone fly-through (e.g. active "
                  "skydiving area). 30s."));
    const auto sim6_label = std::to_string(6) + " - " + _("Mixed");
    list.Append(6, "6",
                sim6_label.c_str(),
                _("Multiple traffic types and an alert zone, "
                  "no warnings. 30s."));

    int result = ComboPicker(_("Simulation scenario"), list,
                             nullptr, true);
    if (result < 0)
      return;

    unsigned scenario = list[result].int_value;
    try {
      MessageOperationEnvironment env;
      device.RunSimulation(scenario, env);
    } catch (OperationCancelled) {
    } catch (...) {
      ShowError(std::current_exception(), _("Error"));
    }
  });
#endif
}

void
ManageRemoteDialog(Device &device /*, const FlarmVersion &version,
                  FlarmHardware &hardware, const FlarmState &state*/)
{
  WidgetDialog dialog(WidgetDialog::Auto{}, UIGlobals::GetMainWindow(),
                      UIGlobals::GetDialogLook(),
                      "SteFly Remote",
                      new ManageRemoteWidget(UIGlobals::GetDialogLook(),
                                            (StickRemoteControl &)device /*,
                                            version, hardware, state*/));
  dialog.AddButton(_("Close"), mrCancel);
  dialog.ShowModal();
}
