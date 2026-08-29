// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "SystemConfigPanel.hpp"
#include "BackendComponents.hpp"
#include "Components.hpp"
#include "Dialogs/Device/DeviceListDialog.hpp"
#include "Language/Language.hpp"
#include "LocalPath.hpp"
#include "SystemConfig.hpp"
#include "UIGlobals.hpp"
#include "Widget/RowFormWidget.hpp"
#include "system/Path.hpp"

/**
 * Settings of the device, not of the profile: they are stored beside
 * the data directory (SystemConfig) and therefore stay behind when
 * the data directory travels to another device.
 */
class SystemConfigPanel final : public RowFormWidget {
  enum ControlIndex {
    XCSOAR_BEHAVIOUR,
    DEVICES_IN_PROFILE,
    DEVICES,
    LOCATION,
  };

public:
  SystemConfigPanel() noexcept
    :RowFormWidget(UIGlobals::GetDialogLook()) {}

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;
};

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

  /* second way to the NMEA devices and their ports: they belong to
     the device just as much as the settings above */
  AddButton(_("Devices"), [](){
    if (backend_components != nullptr &&
        backend_components->device_blackboard != nullptr)
      ShowDeviceList(*backend_components->device_blackboard,
                     backend_components->devices.get());
  });

  /* where these settings live - deliberately outside the data
     directory, so they do not travel with it */
  const Path config_path = GetSystemConfigPath();
  AddMultiLine(config_path == nullptr
               ? _("No place for device settings on this system.")
               : config_path.c_str());
}

bool
SystemConfigPanel::Save(bool &changed) noexcept
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

  if (modified)
    SystemConfig::Save();

  return true;
}

std::unique_ptr<Widget>
CreateSystemConfigPanel()
{
  return std::make_unique<SystemConfigPanel>();
}
