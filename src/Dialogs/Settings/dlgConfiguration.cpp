// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Dialogs/Dialogs.h"
#include "Dialogs/Message.hpp"
#include "Widget/ArrowPagerWidget.hpp"
#include "Widget/CreateWindowWidget.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Look/DialogLook.hpp"
#include "UIGlobals.hpp"
#include "Form/TabMenuDisplay.hpp"
#include "Form/TabMenuData.hpp"
#include "Form/CheckBox.hpp"
#include "Form/Button.hpp"
#include "Screen/Layout.hpp"
#include "Profile/Profile.hpp"
#include "util/Macros.hpp"
#include "Panels/ConfigPanel.hpp"
#include "Panels/PagesConfigPanel.hpp"
#include "Panels/UnitsConfigPanel.hpp"
#include "Panels/TimeConfigPanel.hpp"
#include "Panels/LoggerConfigPanel.hpp"
#include "Panels/AirspaceConfigPanel.hpp"
#include "Panels/SiteConfigPanel.hpp"
#include "Panels/MapDisplayConfigPanel.hpp"
#include "Panels/WaypointDisplayConfigPanel.hpp"
#include "Panels/SymbolsConfigPanel.hpp"
#include "Panels/TerrainDisplayConfigPanel.hpp"
#include "Panels/GlideComputerConfigPanel.hpp"
#include "Panels/WindConfigPanel.hpp"
#include "Panels/SafetyFactorsConfigPanel.hpp"
#include "Panels/RouteConfigPanel.hpp"
#include "Panels/InterfaceConfigPanel.hpp"
#include "Panels/LayoutConfigPanel.hpp"
#include "Panels/GaugesConfigPanel.hpp"
#include "Panels/VarioConfigPanel.hpp"
#include "Panels/TaskRulesConfigPanel.hpp"
#include "Panels/TaskDefaultsConfigPanel.hpp"
#include "Panels/ScoringConfigPanel.hpp"
#include "Panels/InfoBoxesConfigPanel.hpp"
#include "Interface.hpp"
#include "Language/Language.hpp"
#include "Audio/Features.hpp"
#include "UtilsSettings.hpp"
#include "net/http/Features.hpp"
#include "ui/window/SingleWindow.hpp"

#ifdef HAVE_PCM_PLAYER
#include "Panels/AudioVarioConfigPanel.hpp"
#endif

#ifdef HAVE_VOLUME_CONTROLLER
#include "Panels/AudioConfigPanel.hpp"
#endif

#ifdef HAVE_TRACKING
#include "Panels/TrackingConfigPanel.hpp"
#endif

#include "Panels/CloudConfigPanel.hpp"

#if defined(HAVE_PCMET) || defined(HAVE_HTTP)
#include "Panels/WeatherConfigPanel.hpp"
#endif

#include "Panels/WeGlideConfigPanel.hpp"

#if defined(IS_OPENVARIO)
#include "OpenVario/FileMenuWidget.hpp"
#include "OpenVario/SystemSettingsWidget.hpp"
#include "OpenVario/System/SystemMenuWidget.hpp"
#include "OpenVario/DisplaySettingsWidget.hpp"
#include "OpenVario/ExtraWidget.hpp"
#include "OpenVario/System/OpenVarioDevice.hpp"
#include "UIActions.hpp"
#endif

#include <cassert>

static unsigned current_page;

// TODO: eliminate global variables
static ArrowPagerWidget *pager;

static constexpr TabMenuPage basic_pages[] = {
  { N_("Site Files"), CreateSiteConfigPanel },
#ifdef IS_OPENVARIO
  { N_("TestOpenVario"), CreateSystemMenuWidget},
#endif
  { nullptr, nullptr }
};

static constexpr TabMenuPage map_pages[] = {
  { N_("Orientation"), CreateMapDisplayConfigPanel },
  { N_("Elements"), CreateSymbolsConfigPanel },
  { N_("Waypoints"), CreateWaypointDisplayConfigPanel },
  { N_("Terrain"), CreateTerrainDisplayConfigPanel },
  { N_("Airspace"), CreateAirspaceConfigPanel },
  { nullptr, nullptr }
};

static constexpr TabMenuPage computer_pages[] = {
  { N_("Safety Factors"), CreateSafetyFactorsConfigPanel },
  { N_("Glide Computer"), CreateGlideComputerConfigPanel },
  { N_("Wind"), CreateWindConfigPanel },
  { N_("Route"), CreateRouteConfigPanel },
  { N_("Scoring"), CreateScoringConfigPanel },
  { nullptr, nullptr }
};

static constexpr TabMenuPage gauge_pages[] = {
  { N_("FLARM, Other"), CreateGaugesConfigPanel },
  { N_("Vario"), CreateVarioConfigPanel },
#ifdef HAVE_PCM_PLAYER
  { N_("Audio Vario"), CreateAudioVarioConfigPanel },
#endif
  { nullptr, nullptr }
};

static constexpr TabMenuPage task_pages[] = {
  { N_("Task Rules"), CreateTaskRulesConfigPanel },
  { N_("Turnpoint Types"), CreateTaskDefaultsConfigPanel },
  { nullptr, nullptr }
};

static constexpr TabMenuPage look_pages[] = {
  { N_("Language, Input"), CreateInterfaceConfigPanel },
  { N_("Screen Layout"), CreateLayoutConfigPanel },
  { N_("Pages"), CreatePagesConfigPanel },
  { N_("InfoBox Sets"), CreateInfoBoxesConfigPanel },
  { nullptr, nullptr }
};

static constexpr TabMenuPage setup_pages[] = {
  { N_("Logger"), CreateLoggerConfigPanel },
  { N_("Units"), CreateUnitsConfigPanel },
  // Important: all pages after Units in this list must not have data fields that are
  // unit-dependent because they will be saved after their units may have changed.
  // ToDo: implement API that controls order in which pages are saved
  { N_("Time"), CreateTimeConfigPanel },
#ifdef HAVE_TRACKING
  { N_("Tracking"), CreateTrackingConfigPanel },
#endif
  { _T("XCSoar Cloud"), CreateCloudConfigPanel },
#if defined(HAVE_PCMET) || defined(HAVE_HTTP)
  { N_("Weather"), CreateWeatherConfigPanel },
#endif
  { _T("WeGlide"), CreateWeGlideConfigPanel },
#ifdef HAVE_VOLUME_CONTROLLER
  { N_("Audio"), CreateAudioConfigPanel },
#endif
  { nullptr, nullptr }
};


#ifdef IS_OPENVARIO
static constexpr TabMenuPage openvario_pages[] = {
    {N_("System Settings"), CreateSystemSettingsWidget},
    {N_("Display Settings"), CreateDisplaySettingsWidget},
    {N_("File Transfer"), CreateFileMenuWidget},
    {N_("Advanced Menu (temp)"), CreateExtraWidget},
    // {N_("Advanced Settings (temp)"), CreateSystemMenuWidget},
  { nullptr, nullptr }
};
#endif

static constexpr TabMenuGroup main_menu_captions[] = {
  { N_("Basic Settings"), basic_pages},
  { N_("Map Display"), map_pages },
  { N_("Glide Computer"), computer_pages },
  { N_("Gauges"), gauge_pages },
  { N_("Task Defaults"), task_pages },
  { N_("Look"), look_pages },
  { N_("Setup"), setup_pages },
#ifdef IS_OPENVARIO
  { N_("OpenVario"), openvario_pages},
#endif
};

static void
OnUserLevel(bool expert) noexcept;

#ifdef __AUGUST__
static void OnXCSoarStyle(bool expert) noexcept;
#endif

class ConfigurationExtraButtons final
  : public NullWidget {
  struct Layout {
#ifdef __AUGUST__
    PixelRect expert, xcsoar_style, button2, button1;

    Layout(const PixelRect &rc)
        : expert(rc), xcsoar_style(rc), button2(rc), button1(rc) {
#else
    PixelRect expert, button2, button1;

    Layout(const PixelRect &rc)
        : expert(rc), button2(rc), button1(rc) {
#endif
      const unsigned height = rc.GetHeight();
      const unsigned max_control_height = ::Layout::GetMaximumControlHeight();

      if (height >= 3 * max_control_height) {
        expert.bottom = expert.top + max_control_height;

#ifdef __AUGUST__
        xcsoar_style.top = expert.bottom;
        xcsoar_style.bottom = xcsoar_style.top + max_control_height;
#endif

        button1.top = button2.bottom = rc.bottom - max_control_height;
        button2.top = button2.bottom - max_control_height;
      } else {
        expert.right = button2.left = unsigned(rc.left * 2 + rc.right) / 3;
#ifdef __AUGUST__
        xcsoar_style.right = expert.right;
#endif
        button2.right = button1.left = unsigned(rc.left + rc.right * 2) / 3;
      }
    }
  };

  const DialogLook &look;

  CheckBoxControl expert;
#ifdef __AUGUST__
  CheckBoxControl xcsoar_style;
#endif
  Button button2, button1;
  bool borrowed2, borrowed1;

public:
  ConfigurationExtraButtons(const DialogLook &_look)
    :look(_look),
     borrowed2(false), borrowed1(false) {}

  Button &GetButton(unsigned number) {
    switch (number) {
    case 1:
      return button1;

    case 2:
      return button2;

    default:
      assert(false);
      gcc_unreachable();
    }
  }

protected:
  /* virtual methods from Widget */
  PixelSize GetMinimumSize() const noexcept override {
    return {
      CheckBoxControl::GetMinimumWidth(look,
                                       ::Layout::GetMaximumControlHeight(),
                                       _("Expert")),
      ::Layout::GetMaximumControlHeight() * 3,
    };
  }

  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override {
    Layout layout(rc);

    WindowStyle style;
    style.Hide();
    style.TabStop();

    expert.Create(parent, look, _("Expert"),
                  layout.expert, style,
                  [](bool value){ OnUserLevel(value); });
#ifdef __AUGUST__
    xcsoar_style.Create(parent, look, _("XCSoar"),
                  layout.xcsoar_style, style,
                  [](bool value){ OnXCSoarStyle(value); });
#endif
    button2.Create(parent, look.button, _T(""), layout.button2, style);
    button1.Create(parent, look.button, _T(""), layout.button1, style);
  }

  void Show(const PixelRect &rc) noexcept override {
    Layout layout(rc);

    expert.SetState(CommonInterface::GetUISettings().dialog.expert);
    expert.MoveAndShow(layout.expert);
#ifdef __AUGUST__
    xcsoar_style.SetState(CommonInterface::GetUISettings().dialog.xcsoar_style);
    xcsoar_style.MoveAndShow(layout.xcsoar_style);
#endif

    if (borrowed2)
      button2.MoveAndShow(layout.button2);
    else
      button2.Move(layout.button2);

    if (borrowed1)
      button1.MoveAndShow(layout.button1);
    else
      button1.Move(layout.button1);
  }

  void Hide() noexcept override {
    expert.FastHide();
#ifdef __AUGUST__
    xcsoar_style.FastHide();
#endif
    button2.FastHide();
    button1.FastHide();
  #ifdef IS_OPENVARIO
    // this is a workaround: the 1st SignalShutdown closed the Setup window
    // the 2nd one here close the executable
    // a directly exit isn't possible
    switch (ContainerWindow::GetExitValue()) {
    case LAUNCH_SHELL:           // 100,
    case START_UPGRADE:          // 111
    case LAUNCH_TOUCH_CALIBRATE: // 112,
      UIActions::SignalShutdown(true);
      break;
    default:
      break;
    }
  #endif
  }

  void Move(const PixelRect &rc) noexcept override {
    Layout layout(rc);
    expert.Move(layout.expert);
#ifdef __AUGUST__
    xcsoar_style.Move(layout.xcsoar_style);
#endif
    button2.Move(layout.button2);
    button1.Move(layout.button1);
  }
};

void
ConfigPanel::BorrowExtraButton(unsigned i, const TCHAR *caption,
                               std::function<void()> callback) noexcept
{
  ConfigurationExtraButtons &extra =
    (ConfigurationExtraButtons &)pager->GetExtra();
  Button &button = extra.GetButton(i);
  button.SetCaption(caption);
  button.SetCallback(std::move(callback));
  button.Show();
}

void
ConfigPanel::ReturnExtraButton(unsigned i)
{
  ConfigurationExtraButtons &extra =
    (ConfigurationExtraButtons &)pager->GetExtra();
  Button &button = extra.GetButton(i);
  button.Hide();
}

static void
OnUserLevel(bool expert) noexcept
{
  CommonInterface::SetUISettings().dialog.expert = expert;
  Profile::Set(ProfileKeys::UserLevel, expert);

  /* force layout update */
  pager->PagerWidget::Move(pager->GetPosition());
}

#ifdef __AUGUST__
static void
OnXCSoarStyle(bool xcsoar_style) noexcept
{
  CommonInterface::SetUISettings().dialog.xcsoar_style = xcsoar_style;
  Profile::Set(ProfileKeys::ExtendMenu, xcsoar_style);

  /* force layout update */
  pager->PagerWidget::Move(pager->GetPosition());
}

 /**
 * save the settings in the dialog from menu page.
 */
static void
OnSaveClicked(WidgetDialog &dialog)
{
  bool changed = false;

  // PagerWidget
//  PagerWidget current = (PagerWidget)dialog.GetWidget();
  if (pager->SaveCurrent(changed))
  // if ((PagerWidget) dialog.GetWidget()).SaveCurrent(changed))
  ;
}
#endif

/**
 * close dialog from menu page.  from content, goes to menu page
 */
static void
OnCloseClicked(WidgetDialog &dialog)
{
  if (pager->GetCurrentIndex() == 0)
    dialog.SetModalResult(mrOK);
  else {

  CreateWindowWidget *w = &(CreateWindowWidget&)pager->GetCurrentWidget();
  bool changed = false;
  if (w->Save(changed))
      //      if (w.prepared && !w.widget->Save(changed))
      ;
       //  return ;

//  return true;

    pager->ClickPage(0);
  }
}

static void
OnPageFlipped(WidgetDialog &dialog, TabMenuDisplay &menu)
{
  menu.OnPageFlipped();

  TCHAR buffer[128];
  const TCHAR *caption = menu.GetCaption(buffer, ARRAY_SIZE(buffer));
  if (caption == nullptr)
    caption = _("Configuration");
  dialog.SetCaption(caption);
}

void dlgConfigurationShowModal()
{
  const DialogLook &look = UIGlobals::GetDialogLook();

  WidgetDialog dialog(WidgetDialog::Full{}, UIGlobals::GetMainWindow(),
                      look, _("Configuration"));

  pager = new ArrowPagerWidget(look.button,
#ifdef __AUGUST__
                               [&dialog](){ OnSaveClicked(dialog); },
#endif
                               [&dialog](){ OnCloseClicked(dialog); },
                               std::make_unique<ConfigurationExtraButtons>(look));

  auto _menu = std::make_unique<TabMenuDisplay>(*pager, look);
  auto &menu = *_menu;
  pager->Add(std::make_unique<CreateWindowWidget>([&_menu](ContainerWindow &parent,
                                                           const PixelRect &rc,
                                                           WindowStyle style) {
    style.TabStop();
    _menu->Create(parent, rc, style);
    return std::move(_menu);
  }));


// #ifdef IS_OPENVARIO
//  ovdevice.Initialise();
// #endif

  menu.InitMenu(main_menu_captions, ARRAY_SIZE(main_menu_captions));

  /* restore last selected menu item */
  menu.SetCursor(current_page);

  pager->SetPageFlippedCallback([&dialog, &menu](){
    OnPageFlipped(dialog, menu);
  });

  dialog.FinishPreliminary(pager);

  dialog.ShowModal();

  /* save page number for next time this dialog is opened */
  current_page = menu.GetCursor();

  if (dialog.GetChanged()) {
    Profile::Save();
    if (require_restart)
#if defined(IS_OPENVARIO)
      if (ShowMessageBox(
              _("Changes to configuration saved.  Restart OpenSoar "
                "is needed to apply changes. Do you want restart immediately?"),
              _T(""), MB_YESNO | MB_ICONQUESTION) == IDYES) {
        UI::TopWindow::SetExitValue(EXIT_RESTART);
        UIActions::SignalShutdown(true);
      }
#else
      ShowMessageBox(_("Changes to configuration saved.  Restart XCSoar to "
                        "apply changes."),
                      _T(""), MB_OK);
#endif
  }
}
