// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Dialogs/DataManagement/DataManagement.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Widget/RowFormWidget.hpp"
#include "UIGlobals.hpp"
#include "Dialogs/InternalLink.hpp"
#include "Dialogs/Settings/Panels/SiteConfigPanel.hpp"
#include "Look/DialogLook.hpp"
#include "Language/Language.hpp"
#include "Form/Frame.hpp"
#include "Dialogs/FileManager.hpp"
#include "Dialogs/DataManagement/ExportFlightsPanel.hpp"
#include "Dialogs/DataManagement/BackupRestorePanel.hpp"
#include "Dialogs/DataManagement/ImportDataPanel.hpp"
#include "Dialogs/DataManagement/AdvancedFileExplorer.hpp"
#include "Dialogs/Message.hpp"

class DataManagementWidget : public RowFormWidget {
  /**
   * As a page of the configuration dialog, Site Files and the File
   * Manager are pages of their own next to this one, so the buttons
   * that open them would only lead to a second copy.
   */
  const bool in_configuration;

public:
  explicit DataManagementWidget(const DialogLook &look,
                                bool _in_configuration=false) noexcept
    :RowFormWidget(look), in_configuration(_in_configuration) {}

  void Prepare([[maybe_unused]] ContainerWindow &parent,
               [[maybe_unused]] const PixelRect &rc) noexcept override {
    if (!in_configuration) {
      AddButton(C_("Button", "Navigation & Flight Resources"), [](){ ShowConfigPanel(_("Site Files"), CreateSiteConfigPanel); });
      AddButton(C_("Button", "Download manager"), [](){ ShowFileManager(); });
    }
    AddButton(C_("Button", "Export flights"), [](){ ShowExportFlightsDialog(); });
    AddButton(C_("Button", "Import data"), [](){ ShowImportDataDialog(); });
    AddButton(C_("Button", "Backup manager"), [](){ ShowBackupManagerDialog(); });
    AddButton(C_("Button", "Advanced File Explorer"), [](){ ShowAdvancedFileExplorerDialog(); });
  }
};

void
ShowDataManagementDialog()
{
  const DialogLook &look = UIGlobals::GetDialogLook();

  TWidgetDialog<DataManagementWidget> dlg(
    WidgetDialog::Full{}, UIGlobals::GetMainWindow(),
    look, C_("Menu", "Data Management"));

  dlg.AddButton(C_("Button", "Back"), dlg.MakeModalResultCallback(mrCancel));
  dlg.SetWidget(look);
  dlg.ShowModal();
}

std::unique_ptr<Widget>
CreateDataManagementPanel() noexcept
{
  return std::make_unique<DataManagementWidget>(UIGlobals::GetDialogLook(),
                                                true);
}
