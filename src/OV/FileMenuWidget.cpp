// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project
#include "FileMenuWidget.h"
#include "Dialogs/ProcessDialog.hpp"
#include "Hardware/DisplayGlue.hpp"
#include "Language/Language.hpp"

#include <string>

constexpr const char *opensoar = "OpenSoar";
constexpr const char *xcsoar   = "XCSoar";
// std::string 
constexpr const char *main_app = opensoar;

    void
FileMenuWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                        [[maybe_unused]] const PixelRect &rc) noexcept
{
  StaticString<60> title;
  title.Format(_("Download %s IGC files to USB"),main_app);
  AddButton(title, [](){
    static constexpr const char *argv[] = {
            "/usr/bin/download-igc.sh", nullptr
    };
    RunProcessDialog(UIGlobals::GetMainWindow(),
            UIGlobals::GetDialogLook(),
            _("Download IGC Files"), argv);
  });


  title.Format(_("Update or upload %s files from USB"), main_app);
  AddButton(title, []() {
    static constexpr const char *argv[] = {
            "/usr/bin/transfers.sh", "download-data", "main_app.c_str()", nullptr
    };
    
    RunProcessDialog(UIGlobals::GetMainWindow(),
            UIGlobals::GetDialogLook(),
            _("Update/Upload files"), argv);
  });

  title.Format(_("System Backup: OpenVario and %s settings to USB"),  main_app);
  AddButton(title, []() {
    static constexpr const char *argv[] = {
            "/usr/bin/backup-system.sh", nullptr
    };
    
    RunProcessDialog(UIGlobals::GetMainWindow(),
            UIGlobals::GetDialogLook(),
            _("Backup System"), argv);
  });
  
  title.Format(_("System Restore: OpenVario and %s settings from USB"),
               main_app);
  AddButton(title, []() {
     static constexpr const char *argv[] = {
             "/usr/bin/restore-system.sh", nullptr
     };
     
     RunProcessDialog(UIGlobals::GetMainWindow(),
             UIGlobals::GetDialogLook(),
             _("Restore XCSoar and System"), argv);
             Display::Rotate(Display::DetectInitialOrientation());
   });
  
  title.Format(_("%s Restore: Only %s settings from USB"), main_app,
                 main_app );
  AddButton(title, []() {
    static constexpr const char *argv[] = {
             "/usr/bin/transfers.sh", "upload-data", main_app, nullptr
    };
    
    RunProcessDialog(UIGlobals::GetMainWindow(),
            UIGlobals::GetDialogLook(),
            _("Restore XCSoar"), argv);
  });
}