// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ConfigurationConfigPanel.hpp"
// #include "net/client/WeGlide/Settings.hpp"
#include "Profile/Keys.hpp"
#include "Language/Language.hpp"
#include "Widget/RowFormWidget.hpp"
#include "Form/DataField/Boolean.hpp"
#include "Form/DataField/Listener.hpp"
#include "Interface.hpp"
#include "UIGlobals.hpp"

#include <stdio.h>

// #define HAVE_WEGLIDE_PILOTNAME

enum ControlIndex {
  ConfigurationEnabled,
  ConfigurationClubEnabled,
  ConfigurationClubProfile,
  ConfigurationIGCFolder,
};


class ConfigurationConfigPanel final
  : public RowFormWidget, DataFieldListener {
public:
  ConfigurationConfigPanel() noexcept
    :RowFormWidget(UIGlobals::GetDialogLook()) {}

  void SetEnabled(bool enabled) noexcept;

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;

private:
  /* methods from DataFieldListener */
  void OnModified(DataField &df) noexcept override;
};

void
ConfigurationConfigPanel::SetEnabled(bool enabled) noexcept
{
  SetRowEnabled(ConfigurationClubEnabled, enabled);
}

void
ConfigurationConfigPanel::OnModified(DataField &df) noexcept
{
  if (IsDataField(ConfigurationEnabled, df)) {
    const DataFieldBoolean &dfb = (const DataFieldBoolean &)df;
    SetEnabled(dfb.GetValue());
  }
}

void
ConfigurationConfigPanel::Prepare(ContainerWindow &parent,
                            const PixelRect &rc) noexcept
{
  const ConfigurationSettings &configuration = CommonInterface::GetComputerSettings().configuration;

  RowFormWidget::Prepare(parent, rc);

  AddBoolean(
      _("Enable"),
      _("Allow ...."),
      configuration.enabled, this);

  AddBoolean(_("Club Setting Enabled"),
             _("Club."),
             configuration.club_usage_enabled, this);

  AddFile(_("ClubProfile File"),
    _("The Club Profile file.  Support a file reset at every start "
      "to overwrite a changed configuration at the last run."),
    ProfileKeys::ConfigurationClubProfileFile, "*.prf\0",
    FileType::PROFILE);

  AddDirectory(_("IGC file location"), _("IGC file location help"), ProfileKeys::ConfigurationIGCFileFolder);
  
  SetEnabled(configuration.enabled);
}

bool
ConfigurationConfigPanel::Save(bool &_changed) noexcept
{
  bool changed = false;

  auto &configuration = CommonInterface::SetComputerSettings().configuration;

  changed |= SaveValue(ConfigurationEnabled,
                       ProfileKeys::ConfigurationEnabled,
                       configuration.enabled);

  changed |= SaveValue(ConfigurationClubEnabled,
                       ProfileKeys::ConfigurationClubEnabled,
                       configuration.club_usage_enabled);
  
  bool ClubProfileChanged = SaveValueFileReader(ConfigurationClubProfile,
    ProfileKeys::ConfigurationClubProfileFile);

  changed |= ClubProfileChanged;

  _changed |= changed;

  return true;
}

std::unique_ptr<Widget>
CreateConfigurationConfigPanel() noexcept
{
  return std::make_unique<ConfigurationConfigPanel>();
}
