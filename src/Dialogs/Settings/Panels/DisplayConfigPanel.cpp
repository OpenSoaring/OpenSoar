// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "DisplayConfigPanel.hpp"
#include "SystemConfig.hpp"
#include "ui/canvas/Features.hpp" // for DRAW_MOUSE_CURSOR
#include "Profile/Keys.hpp"
#include "Form/DataField/Enum.hpp"
#include "Form/DataField/Boolean.hpp"
#include "Form/DataField/Listener.hpp"
#include "Form/Edit.hpp"
#include "Form/Button.hpp"
#include "Hardware/DisplayBrightness.hpp"
#include "Hardware/RotateDisplay.hpp"
#include "Interface.hpp"
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

#ifdef ANDROID
#include "Android/Main.hpp"
#include "Android/NativeView.hpp"
#endif

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
#ifdef ANDROID
  FullScreen,
#endif
  DarkMode,
  UIScale,
#ifdef DRAW_MOUSE_CURSOR
  CursorSize,
  CursorInverted,
#endif
};

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

static constexpr StaticEnumChoice dark_mode_list[] = {
  { UISettings::DarkMode::AUTO, N_("Auto"),
    N_("Use the system-wide setting") },
  { UISettings::DarkMode::OFF, N_("Off"),
    N_("Black text on white background") },
  { UISettings::DarkMode::ON, N_("On"),
    N_("White text on black background") },
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

#ifdef USE_POLL_EVENT
  UI::event_queue->SetDisplayOrientation(orientation);
#endif

  CommonInterface::main_window->CheckResize();
}

class DisplayConfigPanel final : public RowFormWidget,
                                 DataFieldListener {
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

  /* virtual methods from class DataFieldListener */
  void OnModified(DataField &df) noexcept override;

public:
  DisplayConfigPanel()
    :RowFormWidget(UIGlobals::GetDialogLook()),
     brightness(DisplayBrightness::Detect()) {}

  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;
  void Unprepare() noexcept override;
};

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

#ifdef ANDROID
  AddBoolean(_("Full screen"), _("Run XCSoar in full screen mode"),
             ui_settings.display.full_screen);
#endif

  AddEnum(_("Dark mode"), nullptr, dark_mode_list,
          (unsigned)ui_settings.dark_mode);

  AddInteger(_("Text size"),
             nullptr,
             "%d %%", "%d",
             UISettings::SCALE_MIN, UISettings::SCALE_MAX,
             UISettings::SCALE_STEP,
             ui_settings.scale);

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

  if (touch_screen != nullptr && &df == touch_screen->GetDataField())
    UpdateCalibrateTouch();
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

#ifdef ANDROID
  changed |= SaveValue(FullScreen, ProfileKeys::FullScreen,
                       ui_settings.display.full_screen);
  native_view->SetFullScreen(Java::GetEnv(), ui_settings.display.full_screen);
#endif

  changed |= SaveValueEnum(DarkMode, ProfileKeys::DarkMode,
                           ui_settings.dark_mode);

  if (SaveValueInteger(UIScale, ProfileKeys::UIScale,
                       ui_settings.scale))
    require_restart = changed = true;

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
