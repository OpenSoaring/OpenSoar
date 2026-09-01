// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "InfoBoxPicker.hpp"
#include "InfoBoxGroupPicker.hpp"
#include "InfoBoxes/Content/Factory.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Dialogs/Message.hpp"
#include "Widget/RowFormWidget.hpp"
#include "Form/DataField/Enum.hpp"
#include "Form/DataField/Listener.hpp"
#include "Form/Edit.hpp"
#include "Renderer/TextRowRenderer.hpp"
#include "Look/DialogLook.hpp"
#include "Language/Language.hpp"
#include "UIGlobals.hpp"
#include "ui/control/List.hpp"
#include "util/StringAPI.hxx"

#include <algorithm>
#include <vector>

using InfoBoxFactory::GroupMask;
using InfoBoxFactory::Type;

InfoBoxFactory::GroupMask &
GetInfoBoxGroupFilter() noexcept
{
  static GroupMask mask;
  return mask;
}

/*
 * the "Groups" row
 */

InfoBoxGroupsDataField::InfoBoxGroupsDataField(GroupMask &_mask,
                                               DataFieldListener *listener) noexcept
  :DataFieldString("", listener), mask(_mask)
{
  char buffer[256];
  SetValue(FormatInfoBoxGroups(mask, buffer));
}

void
InfoBoxGroupsDataField::Update() noexcept
{
  char buffer[256];
  ModifyValue(FormatInfoBoxGroups(mask, buffer));
}

bool
EditInfoBoxGroups([[maybe_unused]] const char *caption, DataField &df,
                  [[maybe_unused]] const char *help_text) noexcept
{
  auto &groups = static_cast<InfoBoxGroupsDataField &>(df);
  if (!InfoBoxGroupPicker(groups.GetMask()))
    return false;

  groups.Update();
  return true;
}

/*
 * the picker: groups row on top, list below
 */

class InfoBoxPickerWidget final
  : public RowFormWidget, ListItemRenderer, ListCursorHandler,
    DataFieldListener {
  enum Controls { GROUPS };

  WndForm &dialog;
  const Type current;

  ListControl *list = nullptr;
  TextRowRenderer row_renderer;

  /** the types on display, sorted by name */
  std::vector<Type> items;

public:
  InfoBoxPickerWidget(const DialogLook &look, WndForm &_dialog,
                      Type _current) noexcept
    :RowFormWidget(look), dialog(_dialog), current(_current) {}

  Type GetSelected() const noexcept {
    const unsigned i = list->GetCursorIndex();
    return i < items.size() ? items[i] : current;
  }

  void ShowHelp() noexcept {
    const Type type = GetSelected();
    const char *description = InfoBoxFactory::GetDescription(type);
    ShowMessageBox(description != nullptr ? gettext(description) : "",
                   gettext(InfoBoxFactory::GetName(type)), MB_OK);
  }

private:
  void FillList() noexcept;

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;

  /* virtual methods from class ListItemRenderer */
  void OnPaintItem(Canvas &canvas, const PixelRect rc,
                   unsigned idx) noexcept override {
    if (idx < items.size())
      row_renderer.DrawTextRow(canvas, rc,
                               gettext(InfoBoxFactory::GetName(items[idx])));
  }

  /* virtual methods from class ListCursorHandler */
  bool CanActivateItem([[maybe_unused]] unsigned index) const noexcept override {
    return true;
  }

  void OnActivateItem([[maybe_unused]] unsigned index) noexcept override {
    dialog.SetModalResult(mrOK);
  }

  /* virtual methods from class DataFieldListener */
  void OnModified(DataField &df) noexcept override {
    if (IsDataField(GROUPS, df))
      FillList();
  }
};

void
InfoBoxPickerWidget::FillList() noexcept
{
  const GroupMask &groups = GetInfoBoxGroupFilter();

  items.clear();
  const auto add = [this, &groups](unsigned i){
    const auto type = (Type)i;
    if (type == current) {
      items.push_back(type);
      return;
    }

    if (InfoBoxFactory::IsSelectable(type) &&
        groups.Contains(InfoBoxFactory::GetGroup(type)))
      items.push_back(type);
  };

  for (unsigned i = InfoBoxFactory::MIN_TYPE_VAL; i < InfoBoxFactory::NUM_TYPES; i++)
    add(i);

  for (unsigned i = InfoBoxFactory::OPENSOAR_FIRST; i < InfoBoxFactory::OPENSOAR_END; i++)
    add(i);

  std::sort(items.begin(), items.end(), [](Type a, Type b){
    return StringCollate(gettext(InfoBoxFactory::GetName(a)),
                         gettext(InfoBoxFactory::GetName(b))) < 0;
  });

  list->SetLength(items.size());

  const auto i = std::find(items.begin(), items.end(), current);
  list->SetCursorIndex(i != items.end() ? unsigned(i - items.begin()) : 0);
  list->Invalidate();
}

void
InfoBoxPickerWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                             const PixelRect &rc) noexcept
{
  Add(_("Groups"),
      _("Show only the InfoBoxes of these groups in the list."),
      new InfoBoxGroupsDataField(GetInfoBoxGroupFilter(), this));
  GetControl(GROUPS).SetEditCallback(EditInfoBoxGroups);

  const DialogLook &look = GetLook();
  WindowStyle style;
  style.TabStop();
  auto l = std::make_unique<ListControl>((ContainerWindow &)GetWindow(), look,
                                         rc, style,
                                         row_renderer.CalculateLayout(*look.list.font));
  l->SetItemRenderer(this);
  l->SetCursorHandler(this);
  list = l.get();
  AddRemaining(std::move(l));

  FillList();
}

bool
InfoBoxPicker(const char *caption, Type &type) noexcept
{
  const DialogLook &look = UIGlobals::GetDialogLook();

  TWidgetDialog<InfoBoxPickerWidget>
    dialog(WidgetDialog::Full{}, UIGlobals::GetMainWindow(), look, caption);
  dialog.SetWidget(look, dialog, type);
  InfoBoxPickerWidget &widget = dialog.GetWidget();

  dialog.AddButton(_("Select"), mrOK);
  dialog.AddButton(_("Help"), [&widget](){ widget.ShowHelp(); });
  dialog.AddButton(_("Cancel"), mrCancel);

  if (dialog.ShowModal() != mrOK)
    return false;

  type = widget.GetSelected();
  return true;
}

bool
EditInfoBoxContent(const char *caption, DataField &df,
                   [[maybe_unused]] const char *help_text) noexcept
{
  auto &dfe = static_cast<DataFieldEnum &>(df);
  Type type = (Type)dfe.GetValue();
  if (!InfoBoxPicker(caption, type))
    return false;

  dfe.ModifyValue(unsigned(type));
  return true;
}
