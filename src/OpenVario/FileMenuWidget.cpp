// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#ifdef IS_OPENVARIO

#include "FileMenuWidget.hpp"
#include "Dialogs/ProcessDialog.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Form/Form.hpp"
#include "Hardware/DisplayGlue.hpp"
#include "Hardware/RotateDisplay.hpp"
#include "Language/Language.hpp"
#include "Widget/RowFormWidget.hpp"

#include "UIGlobals.hpp"

// #include "ui/event/Globals.hpp"
// #include "ui/display/Display.hpp"

#include <tchar.h>
#include <string>

constexpr const TCHAR *opensoar = _T("OpenSoar");
constexpr const TCHAR *xcsoar = _T("XCSoar");
constexpr const TCHAR *main_app = opensoar;
constexpr const char *_main_app = "OpenSoar";  // only temporarily

class FileMenuWidget final : public RowFormWidget {
public:
  FileMenuWidget() noexcept : RowFormWidget(UIGlobals::GetDialogLook()) {}

private:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
};

void FileMenuWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                        [[maybe_unused]] const PixelRect &rc) noexcept
{
  StaticString<60> title;
  title.Format(_("Download %s IGC files to USB (WIP)"),main_app);
  AddButton(title, [](){
    static constexpr const char *argv[] = {
            "/usr/bin/download-igc.sh", nullptr
    };
    RunProcessDialog(UIGlobals::GetMainWindow(),
            UIGlobals::GetDialogLook(),
            _("Download IGC Files"), argv);
  });


  title.Format(_("Download %s data files from OV to USB"), main_app);
  AddButton(title, []() {
    static constexpr const char *argv[] = {
            "/usr/bin/transfers.sh", "download-data", _main_app, nullptr
    };
    
    RunProcessDialog(UIGlobals::GetMainWindow(),
            UIGlobals::GetDialogLook(),
            _("Download files"), argv);
  });

  title.Format(_("Restore %s data files from USB"), main_app);
  AddButton(title, []() {
    static constexpr const char *argv[] = {"/usr/bin/transfers.sh",
                                           "upload-data", _main_app, nullptr};

    StaticString<32> dialog_title;
    dialog_title.Format(_("Restore %s"), main_app);
    RunProcessDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
                     dialog_title, argv);
  });

  AddReadOnly(_T("--- System: ---"));

  title.Format(_("System Backup: OpenVario and %s settings to USB"), main_app);
  AddButton(title, []() {
    static constexpr const char *argv[] = {"/usr/bin/transfer-system.sh",
                                           "backup", _main_app, nullptr
    };
    
    RunProcessDialog(UIGlobals::GetMainWindow(),
            UIGlobals::GetDialogLook(),
            _("Backup System"), argv);
  });
  
  title.Format(_("System Restore: OpenVario and %s settings from USB"),
               main_app);
  AddButton(title, []() {
     static constexpr const char *argv[] = {"/usr/bin/transfer-system.sh",
                                           "restore", _main_app, nullptr
     };
     RunProcessDialog(UIGlobals::GetMainWindow(),
             UIGlobals::GetDialogLook(),
             _("Restore System"), argv);
             Display::Rotate(Display::DetectInitialOrientation());
   });
}

bool 
ShowFileMenuWidget(ContainerWindow &parent,
  const DialogLook &look) noexcept
{
  TWidgetDialog<FileMenuWidget> sub_dialog(
      WidgetDialog::Full{}, (UI::SingleWindow &)parent, look,
      _("OpenVario File Transfers"));
  sub_dialog.SetWidget();
  sub_dialog.AddButton(_("Close"), mrOK);
  return sub_dialog.ShowModal();
}

std::unique_ptr<Widget>
CreateFileMenuWidget() noexcept {
  return std::make_unique<FileMenuWidget>();
}
#endif
