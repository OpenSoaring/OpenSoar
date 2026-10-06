// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "OpenVarioSystemWidget.hpp"
#include "Dialogs/Error.hpp"
#include "Dialogs/Message.hpp"
#include "Dialogs/ProcessDialog.hpp"
#include "Form/DataField/Enum.hpp"
#include "Form/Form.hpp"
#include "Interface.hpp"
#include "Language/Language.hpp"
#include "OV/Calibrate.hpp"
#include "OV/System.hpp"
#include "Profile/Profile.hpp"
#include "Profile/ProfileMap.hpp"
#include "UIGlobals.hpp"
#include "Widget/RowFormWidget.hpp"
#include "system/Process.hpp"
#include "ui/display/Display.hpp"
#include "ui/event/Globals.hpp"
#include "ui/event/Queue.hpp"
#include "ui/window/SingleWindow.hpp"

#include <string>

enum ControlIndex {
  IMAGE,
  UPGRADE,
  MAIN_APP,
  CALIBRATE_SENSORS,
  SYSTEM_BACKUP,
  SYSTEM_RESTORE,
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

/**
 * The script of the OpenVario image which copies the system settings
 * and the home directory to the USB stick and back.
 */
static constexpr const char *transfer_system = "/usr/bin/transfer-system.sh";

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
 * Which entry of #main_app_values does the image start?  A missing
 * or unknown value counts as XCSoar: this program is running, so the
 * image has started it.
 */
static unsigned
LoadMainApp() noexcept
try {
  const std::string value = OpenvarioGetMainApp();
  for (unsigned i = 0; i < std::size(main_app_values); ++i)
    if (value == main_app_values[i])
      return i;

  return 0;
} catch (...) {
  return 0;
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

/**
 * Run transfer-system.sh with the given action in a process dialog.
 *
 * @return true if the script succeeded
 */
static bool
RunTransferSystem(const char *caption, const char *action) noexcept
{
  const char *const argv[] = { transfer_system, action, nullptr };
  return RunProcessDialog(UIGlobals::GetMainWindow(),
                          UIGlobals::GetDialogLook(),
                          caption, argv,
                          [](int status){
                            return status == EXIT_SUCCESS ? mrOK : 0;
                          }) == mrOK;
}

/**
 * transfer-system.sh does not check for the USB stick itself; without
 * one it would copy the backup into the empty mount point on the root
 * file system, or restore from there.
 */
static bool
CheckUsbStick(const char *caption) noexcept
{
  if (OpenvarioIsUsbStickMounted())
    return true;

  ShowMessageBox(_("No USB stick found.  Plug in a USB stick and try "
                   "again."),
                 caption, MB_OK | MB_ICONWARNING);
  return false;
}

static void
BackupSystem() noexcept
{
  if (!CheckNotFlying(_("System backup")) ||
      !CheckUsbStick(_("System backup")))
    return;

  /* the backup contains the profile; it should be the one the pilot
     sees, not the one from the last start */
  Profile::Save();

  RunTransferSystem(_("System backup"), "backup");
}

/**
 * The restore writes the settings of the system and of both programs
 * back, the profile in the data directory included.  XCSoar must not
 * save its own profile over it afterwards, and the restored system
 * settings only take effect after a restart; so the device restarts
 * right away.
 */
static void
RestoreSystem() noexcept
{
  if (!CheckNotFlying(_("System restore")) ||
      !CheckUsbStick(_("System restore")))
    return;

  if (ShowMessageBox(_("This overwrites the settings of the system and "
                       "of both programs with the backup on the USB "
                       "stick.  The device restarts afterwards.  "
                       "Continue?"),
                     _("System restore"),
                     MB_YESNO | MB_ICONQUESTION) != IDYES)
    return;

  if (!RunTransferSystem(_("System restore"), "restore"))
    return;

  Profile::SetModified(false);

  /* XCSoar 7.45 has no system power control of its own */
  if (!Run("/sbin/reboot"))
    ShowMessageBox(_("Please restart the device to apply the restored "
                     "settings."),
                   _("System restore"), MB_OK | MB_ICONINFORMATION);
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

  AddButton(_("Back up the system to USB"), BackupSystem);
  AddButton(_("Restore the system from USB"), RestoreSystem);
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
