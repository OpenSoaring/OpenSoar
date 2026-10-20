// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "OpenVarioConfigPanel.hpp"
#include "Dialogs/SystemdService.hpp"
#include "Form/DataField/Boolean.hpp"
#include "Form/DataField/Listener.hpp"
#include "Form/Edit.hpp"
#include "io/FileLineReader.hpp"
#include "util/StringCompare.hxx"
#include "Dialogs/Error.hpp"
#include "Dialogs/JobDialog.hpp"
#include "Dialogs/Message.hpp"
#include "Dialogs/ProcessDialog.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Form/DataField/Enum.hpp"
#include "Form/Form.hpp"
#include "Formatter/ByteSizeFormatter.hpp"
#include "Interface.hpp"
#include "Job/Job.hpp"
#include "Operation/Operation.hpp"
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

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <dirent.h>
#include <sys/statvfs.h>
#include <sys/stat.h>

/**
 * The values of "main_app" which ovmenu-ng.sh knows.  The enum ids
 * index this table.
 */
static constexpr const char *main_app_values[] = {
  "xcsoar",
  "OpenSoar",
};

/** how the programs are called on the screen, by the index of
    #main_app_values */
static constexpr const char *main_app_names[] = {
  "XCSoar",
  "OpenSoar",
};

/**
 * The opkg packages each program may come from, by the index of
 * #main_app_values; the image installs one of them.
 */
static constexpr const char *main_app_packages[][2] = {
  { "xcsoar", "xcsoar-testing" },
  { "opensoar", "opensoar-testing" },
};

/**
 * The script of the OpenVario image which copies the system settings
 * and the home directory to the USB stick and back.
 */
static constexpr const char *transfer_system = "/usr/bin/transfer-system.sh";

class OpenVarioConfigPanel final
  : public RowFormWidget, DataFieldListener {
  /* the rows Prepare() has added, or -1 */
  int main_app_row = -1;
  int sensord_row = -1;
  int variod_row = -1;

  unsigned main_app;

  std::optional<SystemdService> sensord, variod;

public:
  OpenVarioConfigPanel() noexcept
    :RowFormWidget(UIGlobals::GetDialogLook()) {}

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;

private:
  void AddService(int &row, std::optional<SystemdService> &service,
                  const char *id) noexcept;

  /* virtual methods from class DataFieldListener */
  void OnModified(DataField &df) noexcept override;
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
 * Each name in #data_folders becomes a "--data" argument, which makes
 * the backup copy that folder of the data partition as well.
 *
 * @param keep_open leave the dialog open after success as well, so
 * that the pilot can read what was copied; otherwise it closes by
 * itself then, and stays open only to show an error
 * @return true if the script succeeded
 */
static bool
RunTransferSystem(const char *caption, const char *action,
                  bool keep_open = false,
                  const std::vector<std::string> &data_folders = {}) noexcept
{
  std::vector<const char *> argv{transfer_system, action};
  for (const auto &name : data_folders) {
    argv.push_back("--data");
    argv.push_back(name.c_str());
  }
  argv.push_back(nullptr);

  int result = EXIT_FAILURE;
  RunProcessDialog(UIGlobals::GetMainWindow(),
                   UIGlobals::GetDialogLook(),
                   caption, argv.data(),
                   [&result, keep_open](int status){
                     result = status;
                     return status == EXIT_SUCCESS && !keep_open ? mrOK : 0;
                   });
  return result == EXIT_SUCCESS;
}

/**
 * Where ovmenu-ng.sh mounts the data partition.  transfer-system.sh
 * takes the folders to back up by their names below this very path.
 */
static constexpr const char *data_partition = "/home/root/data";

struct DataFolder {
  std::string name;
  uint64_t size = 0;
};

/**
 * The size of all files below a directory.  Symbolic links are
 * counted as nothing and not followed, as rsync copies them.
 */
static uint64_t
GetDirectorySize(const std::string &path,
                 OperationEnvironment &env) noexcept
{
  DIR *dir = opendir(path.c_str());
  if (dir == nullptr)
    return 0;

  uint64_t total = 0;
  while (const struct dirent *entry = readdir(dir)) {
    if (env.IsCancelled())
      break;

    if (StringIsEqual(entry->d_name, ".") ||
        StringIsEqual(entry->d_name, ".."))
      continue;

    const std::string child = path + '/' + entry->d_name;
    struct stat st;
    if (lstat(child.c_str(), &st) < 0)
      continue;

    if (S_ISDIR(st.st_mode))
      total += GetDirectorySize(child, env);
    else if (S_ISREG(st.st_mode))
      total += st.st_size;
  }

  closedir(dir);
  return total;
}

/**
 * Find the folders directly below the data partition and their sizes,
 * so that the pilot sees what a backup of each would cost.  Walking a
 * partition of a few gigabytes takes a moment on the SD card, hence a
 * job.
 */
class ListDataFoldersJob final : public Job {
public:
  std::vector<DataFolder> folders;

  /** the free space on the USB stick, if it could be found out */
  std::optional<uint64_t> usb_free;

  void Run(OperationEnvironment &env) override {
    /* a full stick stopped a backup only after half an hour; the
       pilot should see the room there next to the folder sizes */
    struct statvfs st;
    if (statvfs(openvario_usb_stick, &st) == 0)
      usb_free = uint64_t(st.f_bavail) * st.f_frsize;

    DIR *dir = opendir(data_partition);
    if (dir == nullptr)
      return;

    while (const struct dirent *entry = readdir(dir)) {
      /* "lost+found" belongs to the file system, not to the pilot */
      if (StringIsEqual(entry->d_name, ".") ||
          StringIsEqual(entry->d_name, "..") ||
          StringIsEqual(entry->d_name, "lost+found"))
        continue;

      const std::string path = std::string{data_partition} + '/' +
        entry->d_name;
      struct stat st;
      if (lstat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
        folders.push_back({entry->d_name, 0});
    }
    closedir(dir);

    std::sort(folders.begin(), folders.end(),
              [](const DataFolder &a, const DataFolder &b){
                return a.name < b.name;
              });

    for (auto &folder : folders) {
      if (env.IsCancelled())
        return;
      env.SetText(folder.name.c_str());
      folder.size = GetDirectorySize(std::string{data_partition} + '/' +
                                     folder.name, env);
    }
  }
};

/**
 * Lets the pilot tick the folders of the data partition that the
 * system backup copies besides the settings.  Nothing is ticked at
 * first: maps, images and caches easily come to gigabytes, which take
 * hours on the USB stick.
 */
class DataFolderWidget final : public RowFormWidget {
  const std::vector<DataFolder> &folders;
  const std::optional<uint64_t> usb_free;

  /** the row of the first folder, after the explanation and the
      free space on the stick */
  static constexpr unsigned FIRST_FOLDER = 2;

public:
  DataFolderWidget(const DialogLook &look,
                   const std::vector<DataFolder> &_folders,
                   std::optional<uint64_t> _usb_free) noexcept
    :RowFormWidget(look), folders(_folders), usb_free(_usb_free) {}

  void Prepare([[maybe_unused]] ContainerWindow &parent,
               [[maybe_unused]] const PixelRect &rc) noexcept override {
    AddLabel(_("The settings are always backed up, together with a list "
               "of all files on the data partition.  Tick the folders to "
               "copy as well; large ones take a long time."), 3);

    char free_text[32] = "?";
    if (usb_free)
      FormatByteSize(free_text, sizeof(free_text), *usb_free);
    AddReadOnly(_("Free on the USB stick"), nullptr, free_text);

    for (const auto &folder : folders) {
      char size[32];
      FormatByteSize(size, sizeof(size), folder.size);
      StaticString<256> label;
      label.Format("%s (%s)", folder.name.c_str(), size);
      AddBoolean(label, nullptr, false);
    }
  }

  std::vector<std::string> GetSelected() const noexcept {
    std::vector<std::string> selected;
    for (unsigned i = 0; i < folders.size(); ++i)
      if (GetValueBoolean(FIRST_FOLDER + i))
        selected.push_back(folders[i].name);
    return selected;
  }
};

/**
 * Ask which folders of the data partition go into the backup.
 *
 * @return false if the pilot cancelled the backup
 */
static bool
ChooseDataFolders(std::vector<std::string> &selected) noexcept
{
  const DialogLook &look = UIGlobals::GetDialogLook();

  ListDataFoldersJob job;
  if (!JobDialog(UIGlobals::GetMainWindow(), look,
                 _("System backup"), job, true))
    return false;

  if (job.folders.empty())
    /* no data partition, or nothing on it: just the settings */
    return true;

  TWidgetDialog<DataFolderWidget>
    dialog(WidgetDialog::Auto{}, UIGlobals::GetMainWindow(), look,
           _("System backup"));
  dialog.AddButton(_("Back up"), mrOK);
  dialog.AddButton(_("Cancel"), mrCancel);
  dialog.SetWidget(look, job.folders, job.usb_free);

  if (dialog.ShowModal() != mrOK)
    return false;

  selected = dialog.GetWidget().GetSelected();
  return true;
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

  std::vector<std::string> data_folders;
  if (!ChooseDataFolders(data_folders))
    return;

  /* the backup contains the profile; it should be the one the pilot
     sees, not the one from the last start */
  Profile::Save();

  RunTransferSystem(_("System backup"), "backup", true, data_folders);
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

/**
 * The version of an installed package from the opkg status file, the
 * revision ("-r25.13") left out.  Empty if the package is not
 * installed or the status file cannot be read.
 */
static std::string
GetPackageVersion(const char *package) noexcept
{
  /* where opkg keeps its status on the OpenVario, and on images that
     put it below /usr */
  static constexpr const char *status_files[] = {
    "/var/lib/opkg/status",
    "/usr/lib/opkg/status",
  };

  for (const char *status_file : status_files) {
    try {
      FileLineReaderA reader{Path{status_file}};
      bool in_package = false;
      const char *line;
      while ((line = reader.ReadLine()) != nullptr) {
        if (const char *name = StringAfterPrefix(line, "Package: "))
          in_package = StringIsEqual(name, package);
        else if (in_package)
          if (const char *version = StringAfterPrefix(line, "Version: ")) {
            std::string result = version;
            if (const auto dash = result.rfind('-');
                dash != std::string::npos && dash + 1 < result.size() &&
                result[dash + 1] == 'r')
              result.erase(dash);
            return result;
          }
      }
    } catch (...) {
      /* no such status file; try the next */
    }
  }

  return {};
}

/**
 * "OpenSoar 7.45.25.t04": the name of a program the image can start,
 * with the version installed.  For the running program its own
 * version stands in if opkg does not know it (on a PC, say).
 */
static std::string
MainAppLabel(unsigned i) noexcept
{
  std::string version;
  for (const char *package : main_app_packages[i])
    if (version = GetPackageVersion(package); !version.empty())
      break;

  if (version.empty() && i == RunningApp())
    version = XCSoar_Version;

  std::string label = main_app_names[i];
  if (!version.empty()) {
    label += ' ';
    label += version;
  }
  return label;
}

void
OpenVarioConfigPanel::AddService(int &row,
                                 std::optional<SystemdService> &service,
                                 const char *id) noexcept
{
  service = FindSystemdService(id);
  if (!service)
    return;

  row = GetRowCount();
  AddBoolean(service->display_name, service->description,
             IsSystemdServiceActive(*service), this);
}

void
OpenVarioConfigPanel::Prepare(ContainerWindow &parent,
                              const PixelRect &rc) noexcept
{
  RowFormWidget::Prepare(parent, rc);

  const bool is_openvario = IsOpenVario();

  if (is_openvario) {
    /* the program the image starts, each with the version installed */
    main_app = LoadMainApp();
    main_app_row = GetRowCount();
    WndProperty *wp =
      AddEnum(_("Start after boot"),
              _("The program the OpenVario starts when it is switched on.  "
                "The change takes effect with the next start of the "
                "device."));
    auto &df = *(DataFieldEnum *)wp->GetDataField();
    for (unsigned i = 0; i < std::size(main_app_values); ++i)
      df.AddChoice(i, MainAppLabel(i).c_str());
    df.SetValue(main_app);
    wp->RefreshDisplay();

    /* a click on the image name starts the upgrade */
    const std::string image = OpenvarioGetImageName();
    AddText(_("Firmware"),
            _("The OpenVario image this device is running.  Click to "
              "choose or download another image and upgrade to it: the "
              "program quits and the upgrade starts."),
            image.empty() ? _("Unknown") : image.c_str())
      ->SetEditCallback(EditFirmware);
  }

  /* switching a service on starts it (again) and keeps it on after
     the next boot; switching it off stops it for good */
  AddService(sensord_row, sensord, "sensord");
  AddService(variod_row, variod, "variod");

  if (is_openvario) {
    AddButton(_("Calibrate sensors"), CalibrateSensors);

    AddButton(_("Back up the system to USB"), BackupSystem);
    AddButton(_("Restore the system from USB"), RestoreSystem);
  } else if (!sensord && !variod) {
    AddMultiLine(_("This device is not an OpenVario."));
  }
}

void
OpenVarioConfigPanel::OnModified(DataField &df) noexcept
{
  const auto on = ((const DataFieldBoolean &)df).GetValue();

  for (auto [row, service] : {std::pair{sensord_row, &sensord},
                              std::pair{variod_row, &variod}}) {
    if (row < 0 || !IsDataField(row, df))
      continue;

    /* the field shows what actually runs afterwards, also if the
       switch failed */
    if (const bool running = SwitchSystemdService(**service, on);
        running != on)
      LoadValue(row, running);
    return;
  }
}

bool
OpenVarioConfigPanel::Save(bool &_changed) noexcept
{
  if (main_app_row >= 0 && SaveValueEnum(main_app_row, main_app)) {
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
CreateOpenVarioConfigPanel() noexcept
{
  return std::make_unique<OpenVarioConfigPanel>();
}
