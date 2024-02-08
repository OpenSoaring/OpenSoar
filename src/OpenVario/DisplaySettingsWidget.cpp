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

#include "OpenVario/System/OpenVarioDevice.hpp"
#include "OpenVario/DisplaySettingsWidget.hpp"
#include "OpenVario/System/Setting/RotationWidget.hpp"
#include "OpenVario/System/Setting/WifiWidget.hpp"

#ifndef _WIN32
#include <unistd.h>
#include <sys/stat.h>
#endif
#include <fmt/format.h>

#include <map>
#include <string>

class DisplaySettingsWidget final : public RowFormWidget {
public:
  DisplaySettingsWidget() noexcept
      : RowFormWidget(UIGlobals::GetDialogLook()) {}

private:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
};

void
DisplaySettingsWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                              [[maybe_unused]] const PixelRect &rc) noexcept
{
  AddButton(_("Screen Rotation"), [this](){
    return ShowRotationSettingsWidget(UIGlobals::GetMainWindow(), GetLook());
  });

// AddButton(_("Setting Brightness"), [this](){
//   return ShowSettingBrightnessWidget(UIGlobals::GetMainWindow(), GetLook());
// });

//  uint32_t iTest = 0;
//  AddInteger(_("Brightness Test"), _("Setting Brightness."), _T("%d"), _T("%d"), 1,
//             10, 1, iTest);
}

bool 
ShowDisplaySettingsWidget(ContainerWindow &parent,
                              const DialogLook &look) noexcept {
  TWidgetDialog<DisplaySettingsWidget> sub_dialog(
      WidgetDialog::Full{}, (UI::SingleWindow &)parent, look,
      _T("OpenVario Display Settings"));
  sub_dialog.SetWidget();
  sub_dialog.AddButton(_("Close"), mrOK);
  return sub_dialog.ShowModal();
}

std::unique_ptr<Widget> 
CreateDisplaySettingsWidget() noexcept {
  return std::make_unique<DisplaySettingsWidget>();
}
