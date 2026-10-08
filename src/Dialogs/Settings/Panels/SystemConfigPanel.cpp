// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "SystemConfigPanel.hpp"
#include "BackendComponents.hpp"
#include "Components.hpp"
#include "Dialogs/Device/DeviceListDialog.hpp"
#include "DisplayConfigPanel.hpp"
#include "Form/DataField/Enum.hpp"
#include "Form/Edit.hpp"
#include "Form/DataField/String.hpp"
#include "Device/Register.hpp"
#include "Interface.hpp"
#include "Language/Language.hpp"
#include "SystemConfig.hpp"
#include "UIGlobals.hpp"
#include "Widget/RowFormWidget.hpp"

#include <string>

/**
 * Settings of the device, not of the profile: they are stored beside
 * the data directory (SystemConfig) and therefore stay behind when
 * the data directory travels to another device.
 */
class SystemConfigPanel final : public RowFormWidget {
  enum ControlIndex {
    XCSOAR_BEHAVIOUR,
    DEVICES_IN_PROFILE,
    CUSTOM_DPI,
    DEVICES,
  };

public:
  SystemConfigPanel() noexcept
    :RowFormWidget(UIGlobals::GetDialogLook()) {}

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;
};

/**
 * The enabled devices in the order of their ports, by the names of
 * their drivers ("OpenVario, FLARM"); a port without a driver (the
 * internal GPS, say) by its own name.
 */
static std::string
DescribeDevices() noexcept
{
  std::string text;

  for (const auto &config : CommonInterface::GetSystemSettings().devices) {
    if (config.IsDisabled())
      continue;

    char buffer[128];
    const char *name = config.UsesDriver()
      ? FindDriverDisplayName(config.driver_name)
      : config.GetPortName(buffer, sizeof(buffer));
    if (name == nullptr || *name == '\0')
      continue;

    if (!text.empty())
      text += ", ";
    text += name;
  }

  if (text.empty())
    text = _("None");
  return text;
}

/**
 * Open the device list; afterwards the row shows the devices as they
 * are now.  Signature of WndProperty::EditCallback.
 */
static bool
EditDevices([[maybe_unused]] const char *caption, DataField &df,
            [[maybe_unused]] const char *help_text) noexcept
{
  if (backend_components != nullptr &&
      backend_components->device_blackboard != nullptr)
    ShowDeviceList(*backend_components->device_blackboard,
                   backend_components->devices.get());

  ((DataFieldString &)df).SetValue(DescribeDevices().c_str());
  return true;
}

void
SystemConfigPanel::Prepare(ContainerWindow &parent,
                           const PixelRect &rc) noexcept
{
  RowFormWidget::Prepare(parent, rc);

  AddBoolean(_("XCSoar behaviour"),
             _("Start and leave the program the way XCSoar does it: the "
               "fly/simulator prompt at every start, the profile dialog "
               "only when it is needed and without a countdown, and a "
               "plain question when quitting.  Takes effect on the next "
               "start."),
             SystemConfig::Get().xcsoar_behaviour);

  AddBoolean(_("Devices in the profile"),
             _("Keep the NMEA devices and their ports in the profile, the "
               "way XCSoar does it, instead of in the device port file.  "
               "For a machine that flies with more than one set of "
               "instruments.  Takes effect on the next start."),
             SystemConfig::Get().devices_in_profile);

  /* the resolution belongs to the display, so it is set here; it
     applies from the start screen on, before any profile */
  WndProperty *wp_dpi =
    AddEnum(_("Display resolution"),
            _("The resolution of this display, for displays that report "
              "a wrong size.  It applies from the start screen on; a "
              "profile with a resolution of its own overrides it.  Takes "
              "effect on the next start."));
  FillDpiChoices(*(DataFieldEnum *)wp_dpi->GetDataField(),
                 SystemConfig::Get().custom_dpi);
  wp_dpi->RefreshDisplay();

  /* second way to the NMEA devices and their ports: they belong to
     the device just as much as the settings above; the row shows
     which are in use, a click opens the device list */
  AddText(_("Devices"),
          _("The enabled devices in the order of their ports.  Click to "
            "open the device list."),
          DescribeDevices().c_str())
    ->SetEditCallback(EditDevices);
}

bool
SystemConfigPanel::Save([[maybe_unused]] bool &changed) noexcept
{
  bool modified = false;

  if (const bool xcsoar_behaviour = GetValueBoolean(XCSOAR_BEHAVIOUR);
      xcsoar_behaviour != SystemConfig::Get().xcsoar_behaviour) {
    SystemConfig::Get().xcsoar_behaviour = xcsoar_behaviour;
    modified = true;

    /* "changed" reports profile changes, and this is not one of
       them - the value has just been written to its own file, so the
       flag is deliberately left alone (it is shared with the other
       pages of the dialog) */
  }

  if (const bool devices_in_profile = GetValueBoolean(DEVICES_IN_PROFILE);
      devices_in_profile != SystemConfig::Get().devices_in_profile) {
    SystemConfig::Get().devices_in_profile = devices_in_profile;
    modified = true;
  }

  if (const unsigned custom_dpi = GetValueEnum(CUSTOM_DPI);
      custom_dpi != SystemConfig::Get().custom_dpi) {
    SystemConfig::Get().custom_dpi = custom_dpi;
    modified = true;
  }

  if (modified)
    SystemConfig::Save();

  return true;
}

std::unique_ptr<Widget>
CreateSystemConfigPanel()
{
  return std::make_unique<SystemConfigPanel>();
}
