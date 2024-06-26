// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Dialogs/DialogSettings.hpp"
#include "Dialogs/Message.hpp"
#include "Dialogs/ProcessDialog.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "DisplayOrientation.hpp"
#include "Hardware/DisplayDPI.hpp"
#include "Hardware/DisplayGlue.hpp"
#include "Hardware/RotateDisplay.hpp"
#include "Look/DialogLook.hpp"
#include "Profile/File.hpp"
#include "Profile/Map.hpp"
#include "Screen/Layout.hpp"
#include "UIGlobals.hpp"
#include "system/FileUtil.hpp"
#include "system/Process.hpp"
#include "ui/event/KeyCode.hpp"
#include "ui/event/Timer.hpp"
#include "ui/window/Init.hpp"

#include "Language/Language.hpp"

#include "io/KeyValueFileReader.hpp"
#include "io/FileOutputStream.hxx"
#include "io/BufferedOutputStream.hxx"
#include "io/FileLineReader.hpp"

#include "Widget/RowFormWidget.hpp"


#include "OpenVario/System/OpenVarioDevice.hpp"
#include "OpenVario/System/Setting/RotationWidget.hpp"

#ifndef __MSVC__
#include <unistd.h>
#include <sys/stat.h>
#endif
#include <fmt/format.h>

#include <map>
#include <string>

//----------------------------------------------------------
//----------------------------------------------------------
//----------------------------------------------------------

/* x-menu writes the value for the display rotation to /sys because the value is also required for the console in the OpenVario.
In addition, the display rotation is saved in /boot/config.uEnv so that the Openvario sets the correct rotation again when it is restarted.*/

class SettingRotationWidget final : public RowFormWidget {

public:
  SettingRotationWidget() noexcept
      : RowFormWidget(UIGlobals::GetDialogLook()) {}

private:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  void SaveRotation(const std::string &rotationvalue);
};

/*/
void
SettingRotationWidget::SaveRotation(const int rotationInt)
{

   File::WriteExisting(Path("/sys/class/graphics/fbcon/rotate"), (rotationString).c_str());
//   int rotationInt = stoi(rotationString);
   // ChangeConfigInt("rotation", rotationInt, ovdevice.GetSettingsConfig());
   // TODO(August2111):  move the from ovdevice.settings to ovdevice.sysetm
   LoadConfigFile(ovdevice.system_map, ovdevice.GetSystemConfig());  // -> OpenVarioBaseMenu->StartUp!
   ovdevice.rotation = (DisplayOrientation) stoi(rotationString);
   ovdevice.system_map.insert_or_assign("rotation", std::to_string((unsigned) ovdevice.rotation));
   WriteConfigFile(ovdevice.system_map, ovdevice.GetSystemConfig());
}
*/

void
SettingRotationWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                             [[maybe_unused]] const PixelRect &rc) noexcept
{
 AddButton("Landscape", [this](){
	  // SaveRotation("2");
   // Display::Rotate(DisplayOrientation::LANDSCAPE);
    ovdevice.SetRotation(DisplayOrientation::LANDSCAPE);
 });

 AddButton("Portrait (90°)", [this](){
   //SaveRotation("1");
   ovdevice.SetRotation(DisplayOrientation::REVERSE_PORTRAIT);
 });

 AddButton("Landscape (180°)", [this](){
   // SaveRotation("4");
   ovdevice.SetRotation(DisplayOrientation::REVERSE_LANDSCAPE);
 });

 AddButton("Portrait (270°)", [this](){
   // SaveRotation("3");
   ovdevice.SetRotation(DisplayOrientation::PORTRAIT);
 });
}

bool 
ShowRotationSettingsWidget(ContainerWindow &parent,
                               const DialogLook &look) noexcept {
 TWidgetDialog<SettingRotationWidget> sub_dialog(
     WidgetDialog::Full{}, (UI::SingleWindow &)parent, look,
     "OpenVario Rotation Settings");
 sub_dialog.SetWidget();
 sub_dialog.AddButton(_("Close"), mrOK);
 return sub_dialog.ShowModal();
}
