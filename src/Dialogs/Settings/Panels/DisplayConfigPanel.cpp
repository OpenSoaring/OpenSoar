// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "DisplayConfigPanel.hpp"
#include "SystemConfig.hpp"
#include "ui/canvas/Features.hpp" // for DRAW_MOUSE_CURSOR
#include "ui/window/Features.hpp" // for HAVE_FULL_SCREEN_SETTING
#include "Profile/Keys.hpp"
#include "Profile/Profile.hpp"
#include "Form/DataField/Boolean.hpp"
#include "Form/DataField/Enum.hpp"
#include "Form/DataField/Listener.hpp"
#include "Form/DataField/String.hpp"
#include "Form/Edit.hpp"
#include "Form/Button.hpp"
#include "Hardware/DisplayBrightness.hpp"
#include "Hardware/RotateDisplay.hpp"
#include "Interface.hpp"
#include "DisplaySettings.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Dialogs/HelpDialog.hpp"
#include "Dialogs/Settings/InfoBoxAreaStretchWidget.hpp"
#include "Widget/StaticHelpTextWidget.hpp"
#include "MainWindow.hpp"
#include "LogFile.hpp"

#include <algorithm>
#include <iterator>
#include "Language/Language.hpp"
#include "Widget/RowFormWidget.hpp"
#include "UIGlobals.hpp"
#include "UtilsSettings.hpp"
#include "Asset.hpp"
#include "util/Macros.hpp"
#include "util/StaticString.hxx"

#ifdef USE_POLL_EVENT
#include "ui/event/Globals.hpp"
#include "ui/event/Queue.hpp"
#endif

#if defined(__linux__) && !defined(__ANDROID__) && !defined(KOBO)
#include "OpenVario/Calibrate.hpp"
#include "OpenVario/System.hpp"
#include "ui/event/Globals.hpp"
#endif

#include <memory>
#include <string>

#if defined(KOBO) || (defined(__linux__) && !defined(ANDROID))
#define HAVE_DISPLAY_BRIGHTNESS_CONTROL
#endif

enum ControlIndex {
  AppDisplayType,
  CustomDPI,
  Orientation,
#if defined(HAVE_DISPLAY_BRIGHTNESS_CONTROL)
  ScreenBrightness,
#endif
#ifdef HAVE_FULL_SCREEN_SETTING
  FullScreen,
  InfoBoxAreaStretch,
#endif
#ifdef HAVE_STATUS_BAR_SETTING
  StatusBar,
#endif
#ifdef DRAW_MOUSE_CURSOR
  CursorSize,
  CursorInverted,
#endif
};

#ifdef HAVE_FULL_SCREEN_SETTING
/**
 * The screen edges the InfoBox area can be stretched out to.
 */
static constexpr struct {
  const char *label;
  DisplaySettings::InfoBoxAreaStretchEdge bit;
} infobox_area_stretch_edges[] = {
  { NC_("Screen edge", "Top"),
    DisplaySettings::INFOBOX_AREA_STRETCH_TOP },
  { NC_("Screen edge", "Right"),
    DisplaySettings::INFOBOX_AREA_STRETCH_RIGHT },
  { NC_("Screen edge", "Bottom"),
    DisplaySettings::INFOBOX_AREA_STRETCH_BOTTOM },
  { NC_("Screen edge", "Left"),
    DisplaySettings::INFOBOX_AREA_STRETCH_LEFT },
};

/**
 * A one-line hint for the picker dialog; #TwoWidgets would cut a
 * longer paragraph off in landscape.  The full explanation is the
 * help text of the settings row.
 */
static constexpr const char *infobox_area_stretch_hint =
  N_("Tap an edge to stretch the InfoBox area out to the screen border "
     "there.");

static constexpr const char *infobox_area_stretch_help =
  N_("The InfoBox area is the part of the screen available to the "
     "InfoBoxes, gauges, map overlays and the menu. Pick the edges on "
     "which it is stretched out to the screen border. Unstretched "
     "edges stay clear of the system bars and rounded corners. The "
     "map always uses the whole screen, "
     "and dialogs always stay inside the safe area. Only relevant "
     "while full screen mode is enabled.");

/**
 * Describe the enabled edges for the settings list, e.g. "Top, Bottom".
 */
static std::string
FormatInfoBoxAreaStretch(uint8_t edges) noexcept
{
  if (edges == DisplaySettings::INFOBOX_AREA_STRETCH_NONE)
    return _("None");

  std::string result;

  for (const auto &i : infobox_area_stretch_edges) {
    if ((edges & i.bit) == 0)
      continue;

    if (!result.empty())
      result += ", ";
    result += gettext_context("Screen edge", i.label);
  }

  return result;
}

static bool
EditInfoBoxAreaStretch(const char *caption, DataField &df,
                    [[maybe_unused]] const char *help_text)
{
  DisplaySettings &settings = CommonInterface::SetUISettings().display;

  auto picker =
    std::make_unique<InfoBoxAreaStretchWidget>(UIGlobals::GetDialogLook(),
                                            settings.infobox_area_stretch);
  auto &picker_ref = *picker;

  TWidgetDialog<StaticHelpTextWidget>
    dialog(WidgetDialog::Full{}, UIGlobals::GetMainWindow(),
           UIGlobals::GetDialogLook(), _("Stretch InfoBox area"));
  dialog.SetWidget(std::move(picker), gettext(infobox_area_stretch_hint));
  dialog.AddButton(_("OK"), mrOK);

  /* the row's help text is not reachable from here: WndProperty only
     falls back to OnHelp() when there is no edit callback */
  dialog.AddButton(_("Help"), [caption](){
    HelpDialog(caption, gettext(infobox_area_stretch_help));
  });

  dialog.AddButton(_("Cancel"), mrCancel);

  if (dialog.ShowModal() != mrOK)
    return false;

  const uint8_t edges = picker_ref.GetEdges();
  if (edges == settings.infobox_area_stretch)
    return false;

  settings.infobox_area_stretch = edges;
  Profile::Set(ProfileKeys::InfoBoxAreaStretch, unsigned(edges));
  Profile::Save();

  /* in "Auto" mode the status bar follows the top edge, and the area
     the InfoBoxes and gauges may use changed */
  CommonInterface::main_window->ApplyFullScreenSettings();
  CommonInterface::main_window->ReinitialiseLayout();

  ((DataFieldString &)df).ModifyValue(FormatInfoBoxAreaStretch(edges).c_str());
  return true;
}

#endif

#ifdef HAVE_STATUS_BAR_SETTING
static constexpr StaticEnumChoice status_bar_list[] = {
  { DisplaySettings::StatusBar::AUTO, NC_("Setting", "Auto"),
    N_("Show the status bar unless the InfoBox area is stretched to the "
       "top screen edge, where it would cover the InfoBoxes.") },
  { DisplaySettings::StatusBar::VISIBLE, N_("Visible"),
    N_("Always show the status bar, even in full screen mode. The "
       "InfoBoxes cannot be drawn behind it, so they keep clear of the "
       "top screen edge.") },
  { DisplaySettings::StatusBar::HIDDEN, N_("Hidden"),
    N_("Never show the status bar.") },
  nullptr
};
#endif

static constexpr StaticEnumChoice display_type_list[] = {
  { DisplayType::LCD, NC_("Setting", "LCD"),
    N_("Conventional LCD or OLED. Full scrolling animations.") },
  { DisplayType::E_INK, NC_("Setting", "E-ink"),
    N_("Monochrome electronic paper. Disables kinetic and smooth "
       "scrolling.") },
  { DisplayType::COLOR_E_INK, NC_("Setting", "Color e-ink"),
    N_("Color electronic paper. Disables kinetic and smooth "
       "scrolling like monochrome e-ink.") },
  nullptr
};

static_assert(ARRAY_SIZE(display_type_list) ==
              unsigned(DisplayType::COUNT) + 1,
              "display_type_list must match DisplayType::COUNT");

static constexpr StaticEnumChoice display_orientation_list[] = {
  { DisplayOrientation::DEFAULT,
    N_("Default") },
  { DisplayOrientation::PORTRAIT,
    N_("Portrait") },
  { DisplayOrientation::LANDSCAPE,
    N_("Landscape") },
  { DisplayOrientation::REVERSE_PORTRAIT,
    N_("Reverse Portrait") },
  { DisplayOrientation::REVERSE_LANDSCAPE,
    N_("Reverse Landscape") },
  nullptr
};

void
FillDpiChoices(DataFieldEnum &df, unsigned value,
               unsigned device_dpi) noexcept
{
  /* some displays report a wrong physical size, so the automatic
     resolution is off; a value with a display named after it is the
     measured resolution of that display, so the pilot does not have
     to measure it again */
  static constexpr struct {
    unsigned dpi;
    const char *display;
  } dpi_choices[] = {
    { 120, nullptr },

    /* 5.7" panel with 640 x 480 pixels and a visible area of
       115 x 87 mm (SteFlyNav, Android), measured 141 dpi; 140 is the
       density the device itself is set to, and the difference does
       not show */
    { 140, "5.7\", 640x480 px" },

    { 160, nullptr },

    /* 7.0" panel with 1024 x 600 pixels (SteFlyNav 7.0", Android): the
       device is set to a density of 180 (ro.sf.lcd_density) and
       reports the placeholder 213 dpi; from the diagonal it has about
       170 dpi */
    { 180, "7.0\", 1024x600 px" },

    { 240, nullptr },
    { 260, nullptr },
    { 280, nullptr },
    { 300, nullptr },
    { 340, nullptr },
    { 360, nullptr },
    { 400, nullptr },
    { 420, nullptr },
    { 520, nullptr },
  };

  if (device_dpi > 0) {
    StaticString<48> buffer;
    buffer.Format("%s (%u dpi)", _("Device setting"), device_dpi);
    df.AddChoice(0, buffer);
  } else
    df.AddChoice(0, _("Automatic"));

  /* a value written into the file by hand (e.g. 145) is not in the
     list; it is offered at its place in the order, so the dialog
     shows it and keeps it */
  const bool listed = value == 0 ||
    std::any_of(std::begin(dpi_choices), std::end(dpi_choices),
                [value](const auto &c){ return c.dpi == value; });
  bool added = listed;

  auto add = [&df](unsigned dpi, const char *display) {
    StaticString<48> buffer;
    buffer.Format(_("%u dpi"), dpi);
    if (display != nullptr)
      buffer.AppendFormat(" (%s)", display);
    df.AddChoice(dpi, buffer);
  };

  for (const auto &c : dpi_choices) {
    if (!added && value < c.dpi) {
      add(value, nullptr);
      added = true;
    }

    add(c.dpi, c.display);
  }

  if (!added)
    add(value, nullptr);

  df.SetValue(value);
}

/**
 * Turn the screen to the given orientation at once.  The settings page
 * calls it as soon as the pilot picks an orientation, so that one sees
 * right away whether it is the right one, as in the menu of the
 * OpenVario image; the profile and the boot configuration are written
 * only when the configuration is closed.
 */
static void
ApplyDisplayOrientation(DisplayOrientation orientation) noexcept
{
  if (!Display::Rotate(orientation))
    LogString("Display rotation failed");

#ifdef SOFTWARE_ROTATE_DISPLAY
  CommonInterface::main_window->SetDisplayOrientation(orientation);
#endif

#ifdef USE_POLL_EVENT
  UI::event_queue->SetDisplayOrientation(orientation);
#endif

  CommonInterface::main_window->CheckResize();
}

class DisplayConfigPanel final
  : public RowFormWidget, DataFieldListener {
  std::unique_ptr<DisplayBrightness> brightness;

  /* OpenVario only: the "Touch screen" switch and the calibration
     button that depends on it; nullptr elsewhere */
  WndProperty *touch_screen = nullptr;
  Button *calibrate_touch = nullptr;

  void UpdateCalibrateTouch() noexcept;

  /* the orientation and the brightness follow the rows at once; when
     the configuration is left without saving, they go back to these */
  DisplayOrientation initial_orientation = DisplayOrientation::DEFAULT;
  int initial_brightness = -1;
  bool saved = false;

public:
  DisplayConfigPanel()
    :RowFormWidget(UIGlobals::GetDialogLook()),
     brightness(DisplayBrightness::Detect()) {}

  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;
  void Unprepare() noexcept override;

private:
  void OnModified(DataField &df) noexcept override;
};

void
DisplayConfigPanel::OnModified(DataField &df) noexcept
{
  if (Display::RotateSupported() && IsDataField(Orientation, df)) {
    ApplyDisplayOrientation((DisplayOrientation)GetValueEnum(Orientation));
    return;
  }

#ifdef HAVE_DISPLAY_BRIGHTNESS_CONTROL
  /* the brightness is a setting of the display itself, nothing is
     stored for it; it changes as the value changes, so that the pilot
     sees the effect */
  if (brightness != nullptr && brightness->IsWritable() &&
      IsDataField(ScreenBrightness, df)) {
    brightness->SetBrightnessPercent(GetValueInteger(ScreenBrightness));
    return;
  }
#endif

  if (touch_screen != nullptr && &df == touch_screen->GetDataField()) {
    UpdateCalibrateTouch();
    return;
  }

#ifdef HAVE_FULL_SCREEN_SETTING
  if (IsDataField(FullScreen, df)) {
    const DataFieldBoolean &dfb = (const DataFieldBoolean &)df;
    SetRowAvailable(InfoBoxAreaStretch, dfb.GetValue());
  }
#else
  (void)df;
#endif
}

void
DisplayConfigPanel::Prepare(ContainerWindow &parent,
                            const PixelRect &rc) noexcept
{
  const UISettings &ui_settings = CommonInterface::GetUISettings();

  RowFormWidget::Prepare(parent, rc);

  initial_orientation = ui_settings.display.orientation;
#ifdef HAVE_DISPLAY_BRIGHTNESS_CONTROL
  if (brightness != nullptr && brightness->IsWritable())
    initial_brightness = brightness->GetBrightnessPercent();
#endif

  AddEnum(C_("Setting", "Display type"),
          _("Select the display technology. E-ink modes disable kinetic "
            "and smooth scrolling for slow refresh screens."),
          display_type_list,
          (unsigned)ui_settings.display.display_type);
  SetExpertRow(AppDisplayType);

  WndProperty *wp_dpi = AddEnum(_("Display resolution"),
                                _("The display resolution is used to adapt line widths, "
                                  "font size, landable size and more."));
  FillDpiChoices(*(DataFieldEnum *)wp_dpi->GetDataField(),
                 ui_settings.custom_dpi, SystemConfig::Get().custom_dpi);
  wp_dpi->RefreshDisplay();
  SetExpertRow(CustomDPI);

  if (Display::RotateSupported())
    AddEnum(_("Display orientation"),
            _("Rotate the display on devices that support it."),
            display_orientation_list,
            (unsigned)ui_settings.display.orientation, this);
  else
    AddDummy();

#ifdef HAVE_DISPLAY_BRIGHTNESS_CONTROL
  if (brightness != nullptr) {
    AddInteger(_("Screen brightness"),
               brightness->IsWritable()
               ? _("Adjust the screen brightness.")
               : _("Screen brightness is read-only because writing requires additional permissions."),
               "%d %%", "%d", 0, 100, 5,
               brightness->GetBrightnessPercent(), this);

    if (!brightness->IsWritable())
      SetReadOnly(ScreenBrightness);
  } else
    AddDummy();
#endif

#ifdef HAVE_FULL_SCREEN_SETTING
  AddBoolean(_("Full screen"),
             _("Let XCSoar use the whole screen, including the areas behind "
               "the system bars and the display cutout. The map fills it "
               "completely, while \"Stretch InfoBox area\" decides which "
               "edges the InfoBoxes, gauges, map overlays and the menu "
               "may reach."),
             ui_settings.display.full_screen, this);

  const auto edges = FormatInfoBoxAreaStretch(ui_settings.display
                                          .infobox_area_stretch);
  auto *edges_row = Add(_("Stretch InfoBox area"),
                        gettext(infobox_area_stretch_help),
                        new DataFieldString(edges.c_str()));
  edges_row->SetEditCallback(EditInfoBoxAreaStretch);
  SetExpertRow(InfoBoxAreaStretch);
  SetRowAvailable(InfoBoxAreaStretch, ui_settings.display.full_screen);
#endif

#ifdef HAVE_STATUS_BAR_SETTING
  AddEnum(_("System status bar"),
          _("Whether the system status bar with the clock and the battery "
            "level stays visible."),
          status_bar_list,
          unsigned(ui_settings.display.status_bar));
#endif

#ifdef DRAW_MOUSE_CURSOR
  AddInteger(_("Cursor zoom"), _("Cursor zoom factor"), "%d x", "%d x",
             1, 10, 1, (unsigned)ui_settings.display.cursor_size);
  AddBoolean(_("Invert cursor color"), _("Enable black cursor"),
             ui_settings.display.invert_cursor_colors);
#endif

#if defined(__linux__) && !defined(__ANDROID__) && !defined(KOBO)
  /* the touch screen of an OpenVario is calibrated by a script of the
     image; the button sits here because the calibration belongs to
     the screen, and it is the last row so that the indices above do
     not depend on it */
  if (IsOpenVario()) {
    touch_screen =
      AddBoolean(_("Touch screen"),
                 _("Is a touch panel connected?  The OpenVario cannot find "
                   "out by itself.  Off, the controls are drawn for keys and "
                   "the remote stick, and the touch screen cannot be "
                   "calibrated.  The controls change on the next start."),
                 SystemConfig::Get().touch_screen, this);

    calibrate_touch = AddButton(_("Calibrate touch screen"), []{
      CalibrateTouch(UIGlobals::GetMainWindow().GetDisplay(),
                     *UI::event_queue);
    });
    UpdateCalibrateTouch();
  }
#endif
}

void
DisplayConfigPanel::UpdateCalibrateTouch() noexcept
{
  if (touch_screen == nullptr || calibrate_touch == nullptr)
    return;

#if defined(__linux__) && !defined(__ANDROID__) && !defined(KOBO)
  /* only with a panel, and without a touch input device the script
     could only fail */
  const auto &df = (const DataFieldBoolean &)*touch_screen->GetDataField();
  calibrate_touch->SetEnabled(df.GetValue() && HasTouchScreenDevice());
#endif
}

void
DisplayConfigPanel::Unprepare() noexcept
{
  if (!saved) {
    /* left without saving: undo what OnModified() did */
    if (Display::RotateSupported() &&
        GetValueEnum(Orientation) != unsigned(initial_orientation))
      ApplyDisplayOrientation(initial_orientation);

#ifdef HAVE_DISPLAY_BRIGHTNESS_CONTROL
    if (initial_brightness >= 0 &&
        GetValueInteger(ScreenBrightness) != initial_brightness)
      brightness->SetBrightnessPercent(initial_brightness);
#endif
  }

  RowFormWidget::Unprepare();
}

bool
DisplayConfigPanel::Save(bool &_changed) noexcept
{
  bool changed = false;
  saved = true;

  UISettings &ui_settings = CommonInterface::SetUISettings();

  if (SaveValueEnum(AppDisplayType, ProfileKeys::DisplayType,
                    ui_settings.display.display_type)) {
    changed = true;
    SetDisplayType(ui_settings.display.display_type);
    /* Reload 1-bit vs anti-aliased glyphs (Kobo mono path). */
    CommonInterface::main_window->Initialise();
    CommonInterface::main_window->ReinitialiseLook();
  }

  if (SaveValueEnum(CustomDPI, ProfileKeys::CustomDPI,
                    ui_settings.custom_dpi))
    require_restart = changed = true;

  if (Display::RotateSupported() &&
      SaveValueEnum(Orientation, ProfileKeys::MapOrientation,
                    ui_settings.display.orientation)) {
    changed = true;

    /* OnModified() has turned the screen already while the page was
       open; turning it once more to the same orientation does no harm */
    ApplyDisplayOrientation(ui_settings.display.orientation);

#if defined(__linux__) && !defined(__ANDROID__) && !defined(KOBO)
    /* the OpenVario image rotates the console at boot, before
       XCSoar runs; without this, the boot screen and the menu of the
       image would come up in the old orientation.  "Default" means
       "as the system is set up", so it leaves the setting alone. */
    if (IsOpenVario() &&
        ui_settings.display.orientation != DisplayOrientation::DEFAULT) {
      try {
        OpenvarioSaveRotation(ui_settings.display.orientation);
      } catch (...) {
        LogError(std::current_exception(),
                 "Failed to store the rotation in /boot/config.uEnv");
      }
    }
#endif
  }

#ifdef HAVE_DISPLAY_BRIGHTNESS_CONTROL
  if (brightness != nullptr && brightness->IsWritable()) {
    const unsigned old_percent = brightness->GetBrightnessPercent();
    const unsigned new_percent = GetValueInteger(ScreenBrightness);
    if (new_percent != old_percent)
      brightness->SetBrightnessPercent(new_percent);
  }
#endif

#ifdef HAVE_FULL_SCREEN_SETTING
  bool full_screen_changed =
    SaveValue(FullScreen, ProfileKeys::FullScreen,
              ui_settings.display.full_screen);
  /* the per-edge settings are applied by their own dialog */
  changed |= full_screen_changed;

#ifdef HAVE_STATUS_BAR_SETTING
  if (SaveValueEnum(StatusBar, ProfileKeys::StatusBar,
                    ui_settings.display.status_bar)) {
    changed = true;
    full_screen_changed = true;
  }
#endif

  /* this may change the usable screen area, so do it before the
     remaining settings are applied */
  if (full_screen_changed)
    CommonInterface::main_window->ApplyFullScreenSettings();
#endif

#ifdef DRAW_MOUSE_CURSOR
  changed |= SaveValueInteger(CursorSize, ProfileKeys::CursorSize,
                              ui_settings.display.cursor_size);
  CommonInterface::main_window->SetCursorSize(ui_settings.display.cursor_size);

  changed |= SaveValue(CursorInverted, ProfileKeys::CursorColorsInverted,
                       ui_settings.display.invert_cursor_colors);
  CommonInterface::main_window->SetCursorColorsInverted(
    ui_settings.display.invert_cursor_colors);
#endif

  if (touch_screen != nullptr) {
    const auto &df = (const DataFieldBoolean &)*touch_screen->GetDataField();
    if (df.GetValue() != SystemConfig::Get().touch_screen) {
      /* a setting of the device, not of the profile, so "changed" is
         not touched; the controls follow on the next start */
      SystemConfig::Get().touch_screen = df.GetValue();
      SystemConfig::Save();
      require_restart = true;
    }
  }

  _changed |= changed;
  return true;
}

std::unique_ptr<Widget>
CreateDisplayConfigPanel()
{
  return std::make_unique<DisplayConfigPanel>();
}
