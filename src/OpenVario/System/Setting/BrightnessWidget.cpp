// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#if   0   // ndef __MSVC__
#include "lib/dbus/Connection.hxx"
#include "lib/dbus/ScopeMatch.hxx"
#include "lib/dbus/Systemd.hxx"
#endif

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
// #include "Widget/RowFormWidget.hpp"
#include "system/FileUtil.hpp"
#include "system/Process.hpp"
#include "ui/event/KeyCode.hpp"
// #include "ui/event/Queue.hpp"
#include "ui/event/Timer.hpp"
#include "ui/window/Init.hpp"
// #include "ui/window/SingleWindow.hpp"

#include "Language/Language.hpp"

#include "io/KeyValueFileReader.hpp"
#include "io/FileOutputStream.hxx"
#include "io/BufferedOutputStream.hxx"
#include "io/FileLineReader.hpp"

#include "OpenVario/System/System.hpp"
#include "OpenVario/System/Setting/BrightnessWidget.hpp"

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
void SettingBrightnessWidget::SaveBrightness(const std::string &brightness) {
  File::WriteExisting(Path(_T("/sys/class/backlight/lcd/brightness")),
                      (brightness).c_str());
}

void
SettingBrightnessWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                                [[maybe_unused]] const PixelRect &rc) noexcept
{
  AddButton(_T("20"), [this](){
    SaveBrightness("2");
  });

  AddButton(_T("30"), [this](){
    SaveBrightness("3");
  });

  AddButton(_T("40"), [this](){
    SaveBrightness("4");
  });

  AddButton(_T("50"), [this](){
    SaveBrightness("5");
  });

  AddButton(_T("60"), [this](){
    SaveBrightness("6");
  });

  AddButton(_T("70"), [this](){
    SaveBrightness("7");
  });

  AddButton(_T("80"), [this](){
    SaveBrightness("8");
  });

  AddButton(_T("90"), [this](){
    SaveBrightness("9");
  });

  AddButton(_T("100"), [this](){
    SaveBrightness("10");
  });
}

