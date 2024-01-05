// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project
#include "FileMenuWidget.h"
#include "Dialogs/ProcessDialog.hpp"
#include "Hardware/DisplayGlue.hpp"
#include "Language/Language.hpp"

#include <tchar.h>
#include <string>

constexpr const TCHAR *opensoar = _T("OpenSoar");
constexpr const TCHAR *xcsoar = _T("XCSoar");
constexpr const TCHAR *main_app = opensoar;
constexpr const char *_main_app = "OpenSoar";  // only temporarily

    void
FileMenuWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
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