// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "WeatherConfigPanel.hpp"
#include "Profile/Keys.hpp"
#include "Profile/Profile.hpp"
#include "Weather/Settings.hpp"
#include "Widget/RowFormWidget.hpp"
#include "net/http/Features.hpp"
#include "Interface.hpp"
#include "Language/Language.hpp"
#include "UIGlobals.hpp"
#include "util/NumberParser.hpp"
#include "Form/DataField/Enum.hpp"
#include "Form/DataField/Listener.hpp"
#include "DataGlobals.hpp"
#ifdef HAVE_SKYSIGHT
# include "Weather/Skysight/Skysight.hpp"
#endif

#ifdef HAVE_PCMET
// not yet: # define HAVE_PCMET_OVERLAY
#endif

enum ControlIndex {
#ifdef HAVE_PCMET
  PCMET_USER,
  PCMET_PASSWORD,
# ifdef HAVE_PCMET_OVERLAY
  PCMET_FTP_USER,
  PCMET_FTP_PASSWORD,
# endif
#endif

#ifdef HAVE_HTTP
  ENABLE_TIM,
#endif

#ifdef HAVE_SKYSIGHT
  SPACER,
  SKYSIGHT_EMAIL,
  SKYSIGHT_PASSWORD,
  SKYSIGHT_REGION
#endif
};

class WeatherConfigPanel final
  : public RowFormWidget {
public:
  WeatherConfigPanel()
    :RowFormWidget(UIGlobals::GetDialogLook()) {}

public:
  /* methods from Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;
};

#ifdef HAVE_SKYSIGHT
static void
FillRegionControl(WndProperty &wp, [[maybe_unused]] const char *setting)
{
  DataFieldEnum *df = (DataFieldEnum *)wp.GetDataField();
  auto skysight = DataGlobals::GetSkysight();

  for (auto &i : skysight->GetRegions()) {
    auto displaystring = i.second.name;
    if (displaystring.empty())
      displaystring = i.second.id;
    df->addEnumText(i.first.data(), displaystring.data());
  }

  df->SetValue(skysight->GetRegion().c_str());
  wp.RefreshDisplay();
}
#endif

void
WeatherConfigPanel::Prepare(ContainerWindow &parent,
                            const PixelRect &rc) noexcept
{
  const auto &settings = CommonInterface::GetComputerSettings().weather;

  RowFormWidget::Prepare(parent, rc);

#ifdef HAVE_PCMET
  AddText("pc_met Username", "",
          settings.pcmet.www_credentials.username);
  AddPassword("pc_met Password", "",
              settings.pcmet.www_credentials.password);

#ifdef HAVE_PCMET_OVERLAY
  // code disabled because DWD has terminated our access */
  AddText("pc_met FTP Username", "",
          settings.pcmet.ftp_credentials.username);
  AddPassword("pc_met FTP Password", "",
              settings.pcmet.ftp_credentials.password);
#endif
#endif

#ifdef HAVE_HTTP
  AddBoolean("Thermal Information Map",
             _("Show thermal locations downloaded from Thermal Information Map (thermalmap.info)."),
             settings.enable_tim);
#endif

#ifdef HAVE_SKYSIGHT
  AddSpacer();

  AddText("Skysight Email", "The e-mail you use to log in to the skysight.io site.",
          settings.skysight.email);
  AddPassword("Skysight Password", "Your Skysight password.",
              settings.skysight.password);  
  WndProperty *wp = AddEnum("Skysight Region", "The Skysight region to load data for.", (DataFieldListener*)nullptr);
  FillRegionControl(*wp, settings.skysight.region);
#endif
}

bool
WeatherConfigPanel::Save(bool &_changed) noexcept
{
  bool changed = false;

  auto &settings = CommonInterface::SetComputerSettings().weather;

#ifdef HAVE_PCMET
  changed |= SaveValue(PCMET_USER, ProfileKeys::PCMetUsername,
                       settings.pcmet.www_credentials.username);

  changed |= SaveValue(PCMET_PASSWORD, ProfileKeys::PCMetPassword,
                       settings.pcmet.www_credentials.password);

# ifdef HAVE_PCMET_OVERLAY
  // code disabled because DWD has terminated our access */
  changed |= SaveValue(PCMET_FTP_USER, ProfileKeys::PCMetFtpUsername,
                       settings.pcmet.ftp_credentials.username);

  changed |= SaveValue(PCMET_FTP_PASSWORD, ProfileKeys::PCMetFtpPassword,
                       settings.pcmet.ftp_credentials.password);
# endif
#endif

#ifdef HAVE_HTTP
  changed |= SaveValue(ENABLE_TIM, ProfileKeys::EnableThermalInformationMap,
                       settings.enable_tim);
#endif

#ifdef HAVE_SKYSIGHT
  bool skysight_changed = false;
  skysight_changed |= SaveValue(SKYSIGHT_EMAIL, 
    ProfileKeys::SkysightEmail, settings.skysight.email);

  skysight_changed |= SaveValue(SKYSIGHT_PASSWORD, 
    ProfileKeys::SkysightPassword, settings.skysight.password);

  skysight_changed |= SaveValue(SKYSIGHT_REGION, 
    ProfileKeys::SkysightRegion, settings.skysight.region);   

  if (skysight_changed) {
    // new SkySight if the skysight settings changed only
    DataGlobals::GetSkysight()->Init();
  }
  changed |= skysight_changed;
#endif

  _changed |= changed;

  return true;
}

std::unique_ptr<Widget>
CreateWeatherConfigPanel()
{
  return std::make_unique<WeatherConfigPanel>();
}
