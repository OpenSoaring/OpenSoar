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

#include "OpenVario/System/System.hpp"
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
void
SettingRotationWidget::SaveRotation(const std::string &rotationString)
{
   File::WriteExisting(Path(_T("/sys/class/graphics/fbcon/rotate")), (rotationString).c_str());
   int rotationInt = stoi(rotationString);
   ChangeConfigInt("rotation", rotationInt, ovdevice.GetSettingsConfig());
}

void
SettingRotationWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                             [[maybe_unused]] const PixelRect &rc) noexcept
{
 AddButton(_T("Landscape"), [this](){
	SaveRotation("0");
   Display::Rotate(DisplayOrientation::LANDSCAPE);
 });

 AddButton(_T("Portrait (90°)"), [this](){
   SaveRotation("1");
   Display::Rotate(DisplayOrientation::REVERSE_PORTRAIT);
 });

 AddButton(_T("Landscape (180°)"), [this](){
   SaveRotation("2");
   Display::Rotate(DisplayOrientation::REVERSE_LANDSCAPE);
 });

 AddButton(_T("Portrait (270°)"), [this](){
   SaveRotation("3");
   Display::Rotate(DisplayOrientation::PORTRAIT);
 });
}

