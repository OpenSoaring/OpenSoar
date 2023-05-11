// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project
#include "FileMenuWidget.h"
#include "Dialogs/ProcessDialog.hpp"
#include "Hardware/DisplayGlue.hpp"

void
FileMenuWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                        [[maybe_unused]] const PixelRect &rc) noexcept
{
AddButton("Download XCSoar IGC files to USB", [](){
static constexpr const char *argv[] = {
        "/usr/bin/download-igc.sh", nullptr
};
RunProcessDialog(UIGlobals::GetMainWindow(),
        UIGlobals::GetDialogLook(),
"Download IGC Files", argv);
});

AddButton("Update Maps", [](){
static constexpr const char *argv[] = {
        "/usr/bin/update-maps.sh", nullptr
};

RunProcessDialog(UIGlobals::GetMainWindow(),
        UIGlobals::GetDialogLook(),
"Update Maps", argv);
});

AddButton("Update or upload XCSoar files from USB", [](){
static constexpr const char *argv[] = {
        "/usr/bin/upload-xcsoar.sh", nullptr
};

RunProcessDialog(UIGlobals::GetMainWindow(),
        UIGlobals::GetDialogLook(),
"Update/Upload files", argv);
});

AddButton("System Backup: OpenVario and XCSoar settings to USB", [](){
static constexpr const char *argv[] = {
        "/usr/bin/backup-system.sh", nullptr
};

RunProcessDialog(UIGlobals::GetMainWindow(),
        UIGlobals::GetDialogLook(),
"Backup System", argv);
});

AddButton("System Restore: OpenVario and XCSoar settings from USB", [](){
static constexpr const char *argv[] = {
        "/usr/bin/restore-system.sh", nullptr
};

RunProcessDialog(UIGlobals::GetMainWindow(),
        UIGlobals::GetDialogLook(),
"Restore XCSoar and System", argv);
Display::Rotate(Display::DetectInitialOrientation());
});

AddButton("XCSoar Restore: Only XCSoar settings from USB", [](){
static constexpr const char *argv[] = {
        "/usr/bin/restore-xcsoar.sh", nullptr
};

RunProcessDialog(UIGlobals::GetMainWindow(),
        UIGlobals::GetDialogLook(),
"Restore XCSoar", argv);
});
}