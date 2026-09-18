// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#ifdef IS_OPENVARIO
// don't use (and compile) this code outside an OpenVario project!

#include "OpenVario/SystemSettingsWidget.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Widget/RowFormWidget.hpp"
#include "Look/DialogLook.hpp"

#include "Language/Language.hpp"
#include "Form/DataField/Boolean.hpp"
#include "Form/DataField/Listener.hpp"
#include "Form/DataField/Enum.hpp"
#include "Form/DataField/Integer.hpp"
#include "Form/DataField/File.hpp"
#include "Interface.hpp"
#include "UIGlobals.hpp"
#include "ui/window/SingleWindow.hpp"

#include "Dialogs/Message.hpp"
#include "./LogFile.hpp"
#include "UIActions.hpp"
#include "Version.hpp"
#include "system/FileUtil.hpp"
#include "io/FileOutputStream.hxx"
#include "io/BufferedOutputStream.hxx"
#include "ExitValues.hpp"
#include "ProductName.hpp"
#include "Dialogs/Error.hpp"

#include <filesystem>
#include <string>

#include "OpenVario/System/OpenVarioDevice.hpp"
#include "OpenVario/System/OpenVarioTools.hpp"
#include "OpenVario/System/FirmwareImagePicker.hpp"
#include "OpenVario/System/WifiDialogOV.hpp"


#include "util/StringAPI.hxx"

#include <stdio.h>
#include <string.h>

enum ControlIndex {
  FW_VERSION,
  FIRMWARE,
  ENABLED,
  SENSORD,
  VARIOD,
  SSH,
  TIMEOUT,
  WIFI_BUTTON,
  SENSOR_CAL,

#ifdef _DEBUG
  INTEGERTEST,
#endif
};


#include "UIGlobals.hpp"
// -------------------------------------------
class SystemSettingsWidget final : public RowFormWidget, DataFieldListener {
public:
  SystemSettingsWidget() noexcept : RowFormWidget(UIGlobals::GetDialogLook()) {}

  void SetEnabled(bool enabled) noexcept;

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;
  bool CheckChanged(bool &changed) noexcept;


private:
  /* the image the device is running, as the FIRMWARE row shows it;
     a cancelled upgrade puts it back into the row */
  StaticString<0x100> current_image;

  void ShowCurrentImage() noexcept;
  bool WriteUpgradeRequest(Path image) noexcept;
  void StartUpgrade(Path image) noexcept;

  /* methods from DataFieldListener */
  void OnModified(DataField &df) noexcept override;
};

#ifdef OPENVARIOBASEMENU
  static constexpr StaticEnumChoice timeout_list[] = {
    { 0,  "immediately", },
    { 1,  "1s", },
    { 3,  "3s", },
    { 5,  "5s", },
    { 10, "10s", },
    { 30, "30s", },
    { 60, "1min", },
    { -1, "never", },
    nullptr
  };
#endif

  static constexpr StaticEnumChoice enable_list[] = {
    { SSHStatus::ENABLED,   "enabled", },
    { SSHStatus::DISABLED,  "disabled", },
    { SSHStatus::TEMPORARY, "temporary", },
    nullptr
  };

/**
 * "OV-3.2.20.1-CB2-CH57.img.gz" -> "OV-3.2.20.1-CB2-CH57": the row
 * shows the image without its file suffixes, and the comparison with
 * a chosen file has to use the same form.
 */
static std::string
StripImageSuffix(std::string name) noexcept
{
  for (const char *suffix : {".gz", ".img"}) {
    const std::size_t suffix_length = strlen(suffix);
    if (name.size() > suffix_length &&
        name.compare(name.size() - suffix_length, suffix_length, suffix) == 0)
      name.erase(name.size() - suffix_length);
  }
  return name;
}

void
SystemSettingsWidget::SetEnabled([[maybe_unused]] bool enabled) noexcept
{
  // this disabled itself: SetRowEnabled(ENABLED, enabled);
  // SetRowEnabled(BRIGHTNESS, enabled);
#ifdef OPENVARIOBASEMENU
  SetRowEnabled(TIMEOUT, enabled);
#endif
}

void
SystemSettingsWidget::OnModified([[maybe_unused]] DataField &df) noexcept
{
  if (IsDataField(ENABLED, df)) {
    // const DataFieldBoolean &dfb = ;
    SetEnabled(((const DataFieldBoolean &)df).GetValue());
  } else if (IsDataField(FIRMWARE, df)) {
    /* the row is not a setting: choosing an image means "upgrade to
       this one now"; anything else leaves the running image on show */
    const Path image = ((const FileDataField &)df).GetValue();
    if (image == nullptr || image.empty() ||
        StripImageSuffix(image.GetBase().c_str()) == current_image.c_str())
      return;

    StartUpgrade(image);
  }
}

void
SystemSettingsWidget::ShowCurrentImage() noexcept
{
  auto &df = (FileDataField &)GetDataField(FIRMWARE);
  df.ForceModify(Path(current_image.c_str()));
  GetControl(FIRMWARE).RefreshDisplay();
}

/**
 * Tell the wrapper script which image to flash.  OpenSoar cannot run
 * fw-upgrade.sh itself: the upgrade rewrites the root file system, so
 * the program has to be gone first.  It therefore leaves the request
 * in $HOME/fw-upgrade.request (a shell-sourceable "IMAGEFILE=..."
 * line) and quits with START_UPGRADE; ovmenu-ng.sh picks the file up
 * and calls fw-upgrade.sh with the image, which then skips its own
 * selection menu.
 */
bool
SystemSettingsWidget::WriteUpgradeRequest(Path image) noexcept
try {
  /* the script runs from $HOME, but a relative data path would still
     be a guess; hand it an absolute one */
  const auto absolute = std::filesystem::absolute(image.c_str()).string();

  FileOutputStream file(AllocatedPath::Build(ovdevice.GetHomePath(),
                                             Path("fw-upgrade.request")));
  BufferedOutputStream out(file);
  out.Fmt("IMAGEFILE={}\n", absolute);
  out.Flush();
  file.Commit();
  return true;
} catch (...) {
  ShowError(std::current_exception(), _("Upgrade Firmware"));
  return false;
}

void
SystemSettingsWidget::StartUpgrade(Path image) noexcept
{
  StaticString<0x200> text;
  text.Format(_("Upgrade the firmware to\n%s?\n\n%s quits and the upgrade starts; the device reboots afterwards."),
              image.GetBase().c_str(), PRODUCT_NAME);
  if (ShowMessageBox(text, _("Upgrade Firmware"),
                     MB_OKCANCEL | MB_ICONQUESTION) != IDOK ||
      !WriteUpgradeRequest(image)) {
    ShowCurrentImage();
    return;
  }

  ExitToWrapper(START_UPGRADE);
}

void
SystemSettingsWidget::Prepare(ContainerWindow &parent,
                            const PixelRect &rc) noexcept
{
  RowFormWidget::Prepare(parent, rc);

  AddReadOnly(_("Current OpenSoar"), _("Current firmware version of OpenVario"),
              XCSoar_VersionString);
  /* the file field shows the image; the picker behind it is our
     own, because the images live outside the data directory (data/
     images, the USB stick) and the user should see where each one
     comes from */
  AddFile(_("OV-Firmware"),
          _("The firmware image the OpenVario is running. Choose another image to upgrade to it: OpenSoar quits and the upgrade starts."),
          "OVImage", "*.img.gz\0", FileType::IMAGE)
    ->SetEditCallback(PickFirmwareImage);

  /* the row shows the image the device is running, whatever the
     profile remembers from an earlier choice: the first line of
     /boot/image-version-info names its file (reachable on a
     development PC through OPENVARIO_ROOT) */
  {
    char line[0x100];
    if (File::ReadString(ovdevice.MapSystemPath(Path("/boot/image-version-info")),
                         line, sizeof(line))) {
      /* the first line only, without trailing whitespace */
      line[strcspn(line, "\r\n")] = '\0';
      current_image = StripImageSuffix(line).c_str();
    }
    ShowCurrentImage();
  }
  
  AddBoolean(
      _("Settings Enabled"),
      _("Enable the Settings Page"), ovdevice.enabled, this);

   AddBoolean(_("SensorD"), _("Enable the SensorD"), ovdevice.sensord, this);
   AddBoolean(_("VarioD"), _("Enable the VarioD"), ovdevice.variod, this);
   AddEnum(_("SSH"), _("Enable the SSH Connection"), enable_list,
           ovdevice.ssh);

#ifdef OPENVARIOBASEMENU
   AddEnum(_("Program Timeout"), _("Timeout for Program Start."), timeout_list, ovdevice.timeout);
#else
   AddDummy();  // Placeholder for enum enumeration
#endif

   auto btnWifi = AddButton(
       "Settings Wifi", [this]() { 
         ShowWifiDialog();
     });
   btnWifi->SetEnabled(true);  // dependend on availability? Missing: 

   AddButton(_("Calibrate Sensors"), CalibrateSensors);


#ifdef _DEBUG
   AddInteger(_("IntegerTest"),
               _("IntegerTest."), "%d", "%d", 0,
                  99999, 1, ovdevice.iTest);
#endif

   SetEnabled(ovdevice.enabled);

  /* AddFile() takes no listener, and the listener must not be in
     place before every row exists: OnModified() looks rows up by
     index, and ShowCurrentImage() above fires the data field while
     the widget is still being built */
  GetDataField(FIRMWARE).SetListener(this);
}

bool 
SystemSettingsWidget::CheckChanged([[maybe_unused]] bool &_changed) noexcept
{
   return false;
}

bool
SystemSettingsWidget::Save([[maybe_unused]] bool &_changed) noexcept
{
  bool changed = false;
  changed |= SaveValue(ENABLED, "Enabled", ovdevice.enabled, false);

  if (SaveValue(ENABLED, "Enabled", ovdevice.enabled, false)) {
    ovdevice.settings.insert_or_assign("Enabled",
                          ovdevice.enabled ? "True" : "False");
    changed = true;
  }

#ifdef OPENVARIOBASEMENU
  if (SaveValueEnum(TIMEOUT, "Timeout", ovdevice.timeout)) {
    ovdevice.settings.insert_or_assign("Timeout",
      std::to_string(ovdevice.timeout));
    changed = true;
  }
#endif

#ifdef _DEBUG
  if (SaveValueInteger(INTEGERTEST, "iTest", ovdevice.iTest)) {
    ovdevice.settings.insert_or_assign(
        "iTest", std::to_string(ovdevice.iTest));
    changed = true;
  }
#endif

#if 0 // TODO(August2111) Only Test
  ovdevice.settings.insert_or_assign("OpenSoarData",
                                     "E:/Data/OpenSoarData");
#endif

  if (changed) {
    WriteConfigFile(ovdevice.settings, ovdevice.GetSettingsConfig());
  }

  if (SaveValueEnum(SSH, ovdevice.ssh))
    ovdevice.SetSSHStatus((SSHStatus)ovdevice.ssh); 

  if (SaveValue(SENSORD, ovdevice.sensord))
    ovdevice.SetSystemStatus("sensord",  ovdevice.sensord); 

  if (SaveValue(VARIOD, ovdevice.variod)) 
    ovdevice.SetSystemStatus("variod", ovdevice.variod); 

  _changed = changed;
  return true;
}

// this has only defined in OpenVarioBaseMenu...
bool
ShowSystemSettingsWidget(ContainerWindow  &parent,
  const DialogLook &look) noexcept
{
  TWidgetDialog<SystemSettingsWidget> sub_dialog(
      WidgetDialog::Full{}, (UI::SingleWindow &) parent, look,
      "OpenVario System Settings");
  sub_dialog.SetWidget();
  sub_dialog.AddButton(_("Save"), mrOK);
  sub_dialog.AddButton(_("Close"), mrOK);
  return sub_dialog.ShowModal();
}
#endif

std::unique_ptr<Widget>
CreateSystemSettingsWidget() noexcept
{
  return std::make_unique<SystemSettingsWidget>();
}
