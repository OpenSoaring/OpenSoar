// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "OpenVarioSystemWidget.hpp"
#include "Dialogs/Error.hpp"
#include "Dialogs/Message.hpp"
#include "Form/DataField/Enum.hpp"
#include "Interface.hpp"
#include "Language/Language.hpp"
#include "OpenVario/Calibrate.hpp"
#include "OpenVario/System.hpp"
#include "Profile/Profile.hpp"
#include "UIGlobals.hpp"
#include "Version.hpp"
#include "Widget/RowFormWidget.hpp"
#include "system/Process.hpp"
#include "ui/display/Display.hpp"
#include "ui/event/Globals.hpp"
#include "ui/event/Queue.hpp"
#include "ui/window/SingleWindow.hpp"
#include "util/StringAPI.hxx"

#include <string>
#include <string_view>

enum ControlIndex {
  IMAGE,
  UPGRADE,
  MAIN_APP,
  CALIBRATE_SENSORS,
};

/**
 * The values of "main_app" which ovmenu-ng.sh knows.  The enum ids
 * index this table.
 */
static constexpr const char *main_app_values[] = {
  "xcsoar",
  "OpenSoar",
};

static constexpr StaticEnumChoice main_app_list[] = {
  { 0, "XCSoar" },
  { 1, "OpenSoar" },
  nullptr
};

class OpenVarioSystemWidget final : public RowFormWidget {
  unsigned main_app;

public:
  OpenVarioSystemWidget() noexcept
    :RowFormWidget(UIGlobals::GetDialogLook()) {}

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;
};

/**
 * The entry of #main_app_values for the program that is running.
 * The name is taken from the product token, because the PRODUCT_NAME
 * of the build only reaches a few files such as Version.cpp; elsewhere
 * the macro says "XCSoar" even in a renamed build.
 */
[[gnu::pure]]
static unsigned
RunningApp() noexcept
{
  const std::string_view token{XCSoar_ProductToken};
  const auto name = token.substr(0, token.find(' '));

  for (unsigned i = 0; i < std::size(main_app_values); ++i)
    if (name.size() == std::string_view{main_app_values[i]}.size() &&
        StringIsEqualIgnoreCase(name.data(), main_app_values[i],
                                name.size()))
      return i;

  return 0;
}

/**
 * Which entry of #main_app_values does the image start?  ovmenu-ng.sh
 * accepts "XCSoar" as well as "xcsoar", hence the comparison ignores
 * the case.  A missing or unknown value counts as the running
 * program: the image has started it.
 */
static unsigned
LoadMainApp() noexcept
try {
  const std::string value = OpenvarioGetMainApp();
  for (unsigned i = 0; i < std::size(main_app_values); ++i)
    if (StringIsEqualIgnoreCase(value.c_str(), main_app_values[i]))
      return i;

  return RunningApp();
} catch (...) {
  return RunningApp();
}

static bool
CheckNotFlying(const char *caption) noexcept
{
  if (!CommonInterface::Calculated().flight.flying)
    return true;

  ShowMessageBox(_("Not available while flying."), caption,
                 MB_OK | MB_ICONWARNING);
  return false;
}

/**
 * Hand the screen over to the upgrade script of the image.  The
 * script lets the pilot choose an image (from the USB stick or the
 * data partition), writes it to the SD card and restarts the device;
 * cancelling it returns here.  It draws on the console, so XCSoar
 * gives up the display and the input devices until it is done.
 */
static void
UpgradeFirmware() noexcept
{
  if (!CheckNotFlying(_("Upgrade firmware")))
    return;

  if (ShowMessageBox(_("The upgrade script lets you choose an image and "
                       "restarts the device when it is done.  Settings "
                       "changed in this dialog since it was opened are "
                       "lost if the upgrade goes ahead.  Continue?"),
                     _("Upgrade firmware"),
                     MB_YESNO | MB_ICONQUESTION) != IDYES)
    return;

  /* the restart at the end of the upgrade does not give XCSoar the
     chance to save the profile */
  Profile::Save();

  auto &main_window = UIGlobals::GetMainWindow();
  const UI::ScopeDropMaster drop_master{main_window.GetDisplay()};
  const UI::ScopeSuspendEventQueue suspend_event_queue{*UI::event_queue};
  Run("/usr/bin/fw-upgrade.sh");
}

void
OpenVarioSystemWidget::Prepare(ContainerWindow &parent,
                               const PixelRect &rc) noexcept
{
  RowFormWidget::Prepare(parent, rc);

  const std::string image = OpenvarioGetImageName();
  AddReadOnly(_("Firmware image"),
              _("The OpenVario image this device is running."),
              image.empty() ? _("Unknown") : image.c_str());

  AddButton(_("Upgrade firmware"), UpgradeFirmware);

  main_app = LoadMainApp();
  AddEnum(_("Start after boot"),
          _("The program the OpenVario starts when it is switched on.  "
            "The change takes effect with the next start of the device."),
          main_app_list, main_app);

  AddButton(_("Calibrate sensors"), CalibrateSensors);
}

bool
OpenVarioSystemWidget::Save(bool &_changed) noexcept
{
  if (SaveValueEnum(MAIN_APP, main_app)) {
    try {
      OpenvarioSetMainApp(main_app_values[main_app]);
    } catch (...) {
      ShowError(std::current_exception(), _("Start after boot"));
      return false;
    }

    _changed = true;
  }

  return true;
}

std::unique_ptr<Widget>
CreateOpenVarioSystemWidget() noexcept
{
  return std::make_unique<OpenVarioSystemWidget>();
}
