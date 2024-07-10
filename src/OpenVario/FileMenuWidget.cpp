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

constexpr const char *opensoar = "OpenSoar";
constexpr const char *xcsoar = "XCSoar";
constexpr const char *main_app = opensoar;
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
  
  title.Format(_(" - Upload IGC Files to USB (WIP)"), main_app);
  AddLabel(title);

  AddButton(_("Upload IGC Files"), []() {
    static constexpr const char *argv[] = {
            "/usr/bin/download-igc.sh", nullptr
    };
    RunProcessDialog(UIGlobals::GetMainWindow(),
            UIGlobals::GetDialogLook(),
            _("Download IGC Files to USB"), argv);
  });

  //-----------------------------------------------------
  title.Format(_(" - Upload Files to OpenVario from USB (map, profile, etc.)"), main_app);
  AddLabel(title);
  AddButton(_("Upload Files to OpenVario"), []() {
    static constexpr const char *argv[] = {"/usr/bin/transfers.sh",
                                           "upload-data", _main_app, nullptr};

    StaticString<32> dialog_title;
    dialog_title.Format(_("Restore %s"), main_app);
    RunProcessDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
                     dialog_title, argv);
  });

  title.Format(_(" - Backup and Restore OpenVario Settings and Files to USB"), main_app);
  AddLabel(title);

  AddButton(_("Backup OpenSoar"), []() {
    static constexpr const char *argv[] = {
            "/usr/bin/transfers.sh", "download-data", _main_app, nullptr
    };
    
    RunProcessDialog(UIGlobals::GetMainWindow(),
            UIGlobals::GetDialogLook(),
            _("Download files"), argv);
  });

  AddButton(_("Restore OpenSoar"), []() {
    static constexpr const char *argv[] = {"/usr/bin/transfers.sh",
                                           "restore-data", _main_app, nullptr};

    StaticString<32> dialog_title;
    dialog_title.Format(_("Restore %s"), main_app);
    RunProcessDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
                     dialog_title, argv);
  });

  //-----------------------------------------------------
  title.Format(_("- Backup and Restore OpenSoar AND OpenVario System to USB"), main_app);
  AddLabel(title); // "---OpenSoar Data Files---");

  AddButton(_("Backup System"), []() {
    static constexpr const char *argv[] = {"/usr/bin/transfer-system.sh",
                                           "backup", _main_app, nullptr
    };
    
    RunProcessDialog(UIGlobals::GetMainWindow(),
            UIGlobals::GetDialogLook(),
            _("Backup System"), argv);
  });
  
  AddButton(_("Restore System"), []() {
     static constexpr const char *argv[] = {"/usr/bin/transfer-system.sh",
                                           "restore", _main_app, nullptr
     };
     RunProcessDialog(UIGlobals::GetMainWindow(),
             UIGlobals::GetDialogLook(),
             _("Restore System"), argv);
             Display::Rotate(Display::DetectInitialOrientation());
   });
  
  auto btn = AddButton(_("Image Backup to USB (WIP)"), []() {
     static constexpr const char *argv[] = {"/usr/bin/transfer-system.sh",
                                           "restore", _main_app, nullptr
     };
     RunProcessDialog(UIGlobals::GetMainWindow(),
             UIGlobals::GetDialogLook(),
             _("Backup Image"), argv);
             Display::Rotate(Display::DetectInitialOrientation());
   });
  btn->SetEnabled(false);
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

#endif  // IS_OPENVARIO