// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ProfileListDialog.hpp"
#include "ProfilePasswordDialog.hpp"
#include "Dialogs/Error.hpp"
#include "Dialogs/Message.hpp"
#include "Dialogs/TextEntry.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Widget/TextListWidget.hpp"
#include "Form/Button.hpp"
#include "system/FileUtil.hpp"
#include "system/Path.hpp"
#include "LocalPath.hpp"
#include "Profile/Profile.hpp"
#include "Profile/Map.hpp"
#include "Profile/File.hpp"
#include "UIGlobals.hpp"
#include "Language/Language.hpp"
#include "util/StaticString.hxx"
#include "ui/window/TopWindow.hpp"

#include <vector>
#include <cassert>

class ProfileListWidget final
  : public TextListWidget {
  static constexpr char const* extension = ".prf";

  struct ListItem {
    StaticString<32> name;
    AllocatedPath path;

    ListItem(const char *_name, Path _path)
      :name(_name), path(_path) {}

    bool operator<(const ListItem &i2) const {
      return StringCollate(name, i2.name) < 0;
    }
  };

  class ProfileFileVisitor: public File::Visitor
  {
    std::vector<ListItem> &list;

  public:
    ProfileFileVisitor(std::vector<ListItem> &_list):list(_list) {}

    void Visit(Path path, Path filename) override {
      list.emplace_back(filename.c_str(), path);
    }
  };

  const bool select;

  WndForm *form;
  Button *password_button;
  Button *copy_button, *delete_button;
  Button *rename_button;

  std::vector<ListItem> list;

public:
  ProfileListWidget(bool _select):select(_select) {}

  void CreateButtons(WidgetDialog &dialog);

  [[gnu::pure]]
  Path GetSelectedPath() const {
    if (list.empty())
      return nullptr;

    return list[GetList().GetCursorIndex()].path;
  }

  void SelectPath(Path path);
  size_t GetListSize() { return list.size(); }

private:
  void UpdateList();
  uint32_t BestProfileItem();

  [[gnu::pure]]
  int FindPath(Path path) const;

  void SelectClicked();
  void NewClicked();
  void PasswordClicked();
  void CopyClicked();
  void DeleteClicked();
  void RenameClicked();

public:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override;

protected:
  /* virtual methods from TextListWidget */
  const char *GetRowText(unsigned i) const noexcept override {
    return list[i].name;
  }

  /* virtual methods from ListCursorHandler */
  bool CanActivateItem([[maybe_unused]] unsigned index) const noexcept override {
    return select;
  }

  void OnActivateItem([[maybe_unused]] unsigned index) noexcept override {
    form->SetModalResult(mrOK);
  }
};

/* preselect the most recently used profile */
uint32_t
ProfileListWidget::BestProfileItem()
{
  uint32_t best_index = 0;

  std::chrono::system_clock::time_point best_timestamp =
    std::chrono::system_clock::time_point::min();

  int i = -1;
  for (auto &item : list) {
    i++;
    const auto timestamp = File::GetLastModification(item.path);
    if (timestamp > best_timestamp) {
      best_timestamp = timestamp;
      best_index = i;
    }
  }

  return best_index;
}

void
ProfileListWidget::UpdateList()
{
  list.clear();

  ProfileFileVisitor pfv(list);
  VisitDataFiles("*.prf", pfv, false);

  unsigned len = list.size();

  if (len > 0)
    std::sort(list.begin(), list.end());

  ListControl &list_control = GetList();
  list_control.SetLength(len);
  list_control.Invalidate();
  
  const uint32_t index  = BestProfileItem();
  if (index < len) {
    GetList().SetCursorIndex(index);
  }

  const bool empty = list.empty();
  password_button->SetEnabled(!empty);
  copy_button->SetEnabled(!empty);
  delete_button->SetEnabled(!empty);
  rename_button->SetEnabled(!empty);
}

int
ProfileListWidget::FindPath(Path path) const
{
  for (unsigned n = list.size(), i = 0u; i < n; ++i)
    if (path == list[i].path)
      return i;

  return -1;
}

void
ProfileListWidget::SelectPath(Path path)
{
  auto i = FindPath(path);
  if (i >= 0)
    GetList().SetCursorIndex(i);
}

void
ProfileListWidget::CreateButtons(WidgetDialog &dialog)
{
  form = &dialog;
  dialog.AddButton(_("Select"), [this]() { SelectClicked(); });
  dialog.AddButton(_("New"), [this](){ NewClicked(); });
  password_button = dialog.AddButton(_("Password"), [this](){ PasswordClicked(); });
  copy_button = dialog.AddButton(_("Copy"), [this](){ CopyClicked(); });
  delete_button = dialog.AddButton(_("Delete"), [this](){ DeleteClicked(); });
  rename_button = dialog.AddButton(_("Rename"), [this](){ RenameClicked(); });
  dialog.AddButton(_("Cancel"), mrCancel);
}

void
ProfileListWidget::Prepare(ContainerWindow &parent,
                           const PixelRect &rc) noexcept
{
  TextListWidget::Prepare(parent, rc);
  UpdateList();
}

inline void
ProfileListWidget::SelectClicked()
{
  assert(GetList().GetCursorIndex() < list.size());

  const auto &item = list[GetList().GetCursorIndex()];
  std::string new_item = item.name.c_str();
  new_item = new_item.substr(0, new_item.length() - strlen(extension));

  std::string text = "Restart after Selection with profile: " + new_item;
  bool result = ShowMessageBox(text.c_str(), _("Select Profile."), MB_YESNO) == IDYES;

  form->SetModalResult(result ? mrOK : 0);
}

inline void
ProfileListWidget::NewClicked()
{
  StaticString<64> name;
  name.clear();
  if (!TextEntryDialog(name, _("Profile name")))
      return;

  StaticString<80> filename;
  filename = name;
  filename += extension;

  const auto path = LocalPath(filename);
  if (!File::CreateExclusive(path)) {
    ShowMessageBox(name, _("File exists already."), MB_OK|MB_ICONEXCLAMATION);
    return;
  }

  UpdateList();
  SelectPath(path);
}

inline void
ProfileListWidget::PasswordClicked()
{
  assert(GetList().GetCursorIndex() < list.size());

  const auto &item = list[GetList().GetCursorIndex()];

  ProfileMap data;

  try {
    Profile::LoadFile(data, item.path);
  } catch (...) {
    ShowError(std::current_exception(), _("Failed to load file."));
    return;
  }

  if (!CheckProfilePasswordResult(CheckProfilePassword(data)) ||
      !SetProfilePasswordDialog(data))
    return;

  try {
    Profile::SaveFile(data, item.path);
  } catch (...) {
    ShowError(std::current_exception(), _("Failed to save file."));
    return;
  }
}

static bool
ConfirmRenameProfile(const char *name, const char *new_name )
{
  StaticString<256> tmp;
  StaticString<256> tmp_name(name);
  if (tmp_name.length() > 4)
    tmp_name.Truncate(tmp_name.length() - 4);

  tmp.Format(_("Rename '%s' to '%s'?"),
    tmp_name.c_str(), new_name);
  return ShowMessageBox(tmp, _("Rename"), MB_YESNO) == IDYES;
}


inline void
ProfileListWidget::RenameClicked()
{
  assert(GetList().GetCursorIndex() < list.size());

  const auto &item = list[GetList().GetCursorIndex()];
  const Path old_path = item.path;

  ProfileMap data;

  try {
    Profile::LoadFile(data, old_path);
  }
  catch (...) {
    ShowError(std::current_exception(), _("Failed to load file."));
    return;
  }

  if (!CheckProfilePasswordResult(CheckProfilePassword(data)))
    return;

  StaticString<64> new_name;
  new_name.clear();
  if (!TextEntryDialog(new_name, _("Profile name")) || new_name.empty())
    return;

  if (!ConfirmRenameProfile(item.name, new_name))
    return;


  StaticString<80> new_filename;
  new_filename = new_name;
  new_filename += extension;

  const auto new_path = LocalPath(new_filename);

  if (File::ExistsAny(new_path)) {
    ShowMessageBox(new_name, _("File exists already."),
      MB_OK | MB_ICONEXCLAMATION);
    return;
  }

  try {
    File::Rename(old_path, new_path);
  }
  catch (...) {
    ShowError(std::current_exception(), _("Failed to rename file."));
    return;
  }

  UpdateList();
  SelectPath(new_path);
}

inline void
ProfileListWidget::CopyClicked()
{
  assert(GetList().GetCursorIndex() < list.size());

  const auto &item = list[GetList().GetCursorIndex()];
  const Path old_path = item.path;

  ProfileMap data;

  try {
    Profile::LoadFile(data, old_path);
  } catch (...) {
    ShowError(std::current_exception(), _("Failed to load file."));
    return;
  }

  if (!CheckProfilePasswordResult(CheckProfilePassword(data)))
    return;

  StaticString<64> new_name;
  new_name.clear();
  if (!TextEntryDialog(new_name, _("Profile name")) || new_name.empty())
      return;

  StaticString<80> new_filename;
  new_filename = new_name;
  new_filename += extension;

  const auto new_path = LocalPath(new_filename);

  if (File::ExistsAny(new_path)) {
    ShowMessageBox(new_name, _("File exists already."),
                   MB_OK|MB_ICONEXCLAMATION);
    return;
  }

  try {
    data.SetModified(true);
    Profile::SaveFile(data, new_path);
  } catch (...) {
    ShowError(std::current_exception(), _("Failed to save file."));
    return;
  }

  UpdateList();
  SelectPath(new_path);
}

static bool
ConfirmDeleteProfile(const char *name)
{
  StaticString<256> tmp;
  StaticString<256> tmp_name(name);
  if (tmp_name.length() > 4)
    tmp_name.Truncate(tmp_name.length() - 4);

  tmp.Format(_("Delete \"%s\"?"),
             tmp_name.c_str());
  return ShowMessageBox(tmp, _("Delete"), MB_YESNO) == IDYES;
}

inline void
ProfileListWidget::DeleteClicked()
{
  assert(GetList().GetCursorIndex() < list.size());

  const auto &item = list[GetList().GetCursorIndex()];

  try {
    const auto password_result = CheckProfileFilePassword(item.path);
    switch (password_result) {
    case ProfilePasswordResult::UNPROTECTED:
      if (!ConfirmDeleteProfile(item.name))
        return;

      break;

    case ProfilePasswordResult::MATCH:
      break;

    case ProfilePasswordResult::MISMATCH:
    case ProfilePasswordResult::CANCEL:
      CheckProfilePasswordResult(password_result);
      return;
    }
  } catch (...) {
    ShowError(std::current_exception(), _("Password"));
    return;
  }

  File::Delete(item.path);
  UpdateList();
}

void
ProfileListDialog()
{
  auto profile_path = Profile::GetPath();

  TWidgetDialog<ProfileListWidget>
    dialog(WidgetDialog::Full{}, UIGlobals::GetMainWindow(),
           UIGlobals::GetDialogLook(), _("Profiles"));
  dialog.SetWidget(false);
  dialog.GetWidget().CreateButtons(dialog);
  dialog.EnableCursorSelection();

  if (dialog.ShowModal() == mrOK) {
    if (dialog.GetWidget().GetSelectedPath() != profile_path) {
      profile_path = dialog.GetWidget().GetSelectedPath();
      if (!profile_path.empty() && File::Exists(profile_path)) {
        // The profile has been changed
        Profile::Save();  // don't save the changed values in the sel. profile
        Profile::SetFiles(profile_path);  // a 'pre touch'

        File::Touch(profile_path);
        UI::TopWindow::SetExitValue(EXIT_RESTART);
      }
    }
  }
}

AllocatedPath
SelectProfileDialog(Path selected_path)
{
  TWidgetDialog<ProfileListWidget>
    dialog(WidgetDialog::Full{}, UIGlobals::GetMainWindow(),
           UIGlobals::GetDialogLook(), _("Select profile"));
  dialog.SetWidget(true);
  dialog.GetWidget().CreateButtons(dialog);
  dialog.EnableCursorSelection();

  if (selected_path != nullptr) {
    dialog.PrepareWidget();
    dialog.GetWidget().SelectPath(selected_path);
  }

  // with zero or one list entry don't call the dialog
  auto result =
    /* */
    dialog.GetWidget().GetListSize() < 2 ? 
    mrOK : 
 /* */
    dialog.ShowModal();

  return result == mrOK
    ? dialog.GetWidget().GetSelectedPath()
    : nullptr;
}
