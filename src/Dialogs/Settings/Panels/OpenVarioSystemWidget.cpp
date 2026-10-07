// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "OpenVarioSystemWidget.hpp"
#include "Dialogs/Error.hpp"
#include "Dialogs/JobDialog.hpp"
#include "Dialogs/Message.hpp"
#include "Dialogs/ProcessDialog.hpp"
#include "Form/DataField/Enum.hpp"
#include "Form/Form.hpp"
#include "Interface.hpp"
#include "Job/Job.hpp"
#include "UIActions.hpp"
#include "util/StaticString.hxx"
#include "Language/Language.hpp"
#include "OpenVario/Calibrate.hpp"
#include "OpenVario/FirmwareImage.hpp"
#include "OpenVario/FirmwareImagePicker.hpp"
#include "OpenVario/System.hpp"
#include "Profile/Profile.hpp"
#include "Profile/ProfileMap.hpp"
#include "UIGlobals.hpp"
#include "Version.hpp"
#include "Widget/RowFormWidget.hpp"
#include "system/Process.hpp"
#include "ui/window/SingleWindow.hpp"
#include "util/StringAPI.hxx"

#include <string>
#include <string_view>

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
 * The exit code that asks ovmenu-ng.sh for a firmware upgrade
 * (START_UPGRADE of ExitValues.hpp, which defines it on the OpenVario
 * targets only).
 */
static constexpr unsigned EXIT_START_UPGRADE = 205;

/**
 * Put the chosen image into $HOME/data/images, copying it from the
 * USB stick first if necessary, in a thread so the progress shows.
 */
class StageFirmwareJob final : public Job {
  const Path image;

public:
  AllocatedPath entry;
  std::exception_ptr error;

  explicit StageFirmwareJob(Path _image) noexcept:image(_image) {}

  void Run(OperationEnvironment &env) override {
    try {
      entry = StageFirmwareImage(image, env);
    } catch (...) {
      error = std::current_exception();
    }
  }
};

/**
 * Let the pilot choose (or download) an image, put it into
 * $HOME/data/images and quit with EXIT_START_UPGRADE.  The upgrade
 * itself cannot run while the program is still on the screen: the
 * script draws on the console and rewrites the root file system.  So
 * ovmenu-ng.sh starts fw-upgrade.sh with the one image it finds in
 * data/images once the program has ended.
 *
 * Signature of WndProperty::EditCallback.
 */
static bool
EditFirmware([[maybe_unused]] const char *caption,
             [[maybe_unused]] DataField &df,
             [[maybe_unused]] const char *help_text) noexcept
{
  if (!CheckNotFlying(_("Upgrade firmware")))
    return false;

  const auto image = PickFirmwareImage(_("Upgrade firmware"), nullptr);
  if (image == nullptr)
    return false;

  StaticString<0x200> text;
  text.Format(_("Upgrade the firmware to\n%s?\n\n%s quits and the upgrade "
                "starts; the device reboots afterwards."),
              image.GetBase().c_str(), main_app_values[RunningApp()]);
  if (ShowMessageBox(text, _("Upgrade firmware"),
                     MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
    return false;

  StageFirmwareJob job{image};
  JobDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
            _("Upgrade firmware"), job, true);
  if (job.error) {
    ShowError(job.error, _("Upgrade firmware"));
    return false;
  }

  if (job.entry == nullptr)
    /* cancelled while copying */
    return false;

  /* nothing saves the profile once the upgrade has begun */
  Profile::Save();

  ContainerWindow::SetExitValue(EXIT_START_UPGRADE);
  /* no further "Quit?" question: the pilot has confirmed already */
  UIActions::SignalShutdown(true);
  /* SignalShutdown() closes the main window, but while dialogs are
     open that only cancels the top-most one; ending the event loop
     makes every modal loop return, the configuration dialog
     included, and the regular shutdown then runs with the exit value */
  UIGlobals::GetMainWindow().PostQuit();
  return false;
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

  /* a short caption leaves the row's width to the image name; a
     click on it starts the upgrade */
  const std::string image = OpenvarioGetImageName();
  AddText(_("Firmware"),
          _("The OpenVario image this device is running.  Click to choose "
            "or download another image and upgrade to it: the program "
            "quits and the upgrade starts."),
          image.empty() ? _("Unknown") : image.c_str())
    ->SetEditCallback(EditFirmware);

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
