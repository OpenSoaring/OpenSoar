// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Dialogs.h"
#include "Dialogs/Message.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Widget/ListWidget.hpp"
#include "Look/DialogLook.hpp"
#include "UIGlobals.hpp"
#include "Renderer/TextRowRenderer.hpp"
#include "Renderer/TwoTextRowsRenderer.hpp"
#include "Language/Language.hpp"
#include "ActionInterface.hpp"
#include "FrequencyList.hpp"
#include "Profile/Profile.hpp"
#include "Profile/Keys.hpp"
#include "system/Path.hpp"
#include "util/StaticString.hxx"

#include <cassert>

/**
 * The frequency card: the stations of the configured frequency list,
 * one tap away from the standby (or active) frequency of the radio.
 * This is deliberately nothing more than a list - the point of it is
 * that a competition's frequencies are one small file, separate from
 * the checklist and from the waypoint file.
 */
class FrequencyListWidget final : public ListWidget {
  const DialogLook &dialog_look;
  const FrequencyList &stations;

  /* one row per station, or two when at least one station carries a
     comment */
  const bool two_rows;
  TextRowRenderer row_renderer;
  TwoTextRowsRenderer two_rows_renderer;

  Button *standby_button = nullptr;
  Button *close_button = nullptr;

public:
  FrequencyListWidget(const DialogLook &_dialog_look,
                      const FrequencyList &_stations) noexcept
    :dialog_look(_dialog_look), stations(_stations),
     two_rows(HasComments(_stations)) {}

  void CreateButtons(WidgetDialog &dialog) noexcept;

private:
  static bool HasComments(const FrequencyList &list) noexcept {
    for (const auto &i : list)
      if (!i.comment.empty())
        return true;
    return false;
  }

  const RadioStation &GetSelected() const noexcept {
    const unsigned index = GetList().GetCursorIndex();
    assert(index < stations.size());
    return stations[index];
  }

  void SetStandby() noexcept {
    const auto &s = GetSelected();
    ActionInterface::SetStandbyFrequency(s.frequency, s.name.c_str());
    close_button->Click();
  }

  void SetActive() noexcept {
    const auto &s = GetSelected();
    ActionInterface::SetActiveFrequency(s.frequency, s.name.c_str());
    close_button->Click();
  }

public:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;

  /* virtual methods from class ListItemRenderer */
  void OnPaintItem(Canvas &canvas, const PixelRect rc,
                   unsigned index) noexcept override;

  /* virtual methods from class ListCursorHandler */
  bool CanActivateItem([[maybe_unused]] unsigned index) const noexcept override {
    return true;
  }

  void OnActivateItem([[maybe_unused]] unsigned index) noexcept override {
    /* Enter sets the standby frequency: one may still be talking on
       the active one */
    standby_button->Click();
  }
};

void
FrequencyListWidget::CreateButtons(WidgetDialog &dialog) noexcept
{
  standby_button = dialog.AddButton(_("Set Standby Frequency"),
                                    [this](){ SetStandby(); });
  dialog.AddButton(_("Set Active Frequency"),
                   [this](){ SetActive(); });
  close_button = dialog.AddButton(_("Close"), mrCancel);
}

void
FrequencyListWidget::Prepare(ContainerWindow &parent,
                             const PixelRect &rc) noexcept
{
  const unsigned row_height = two_rows
    ? two_rows_renderer.CalculateLayout(*dialog_look.list.font_bold,
                                        dialog_look.small_font)
    : row_renderer.CalculateLayout(*dialog_look.list.font_bold);

  CreateList(parent, dialog_look, rc, row_height);
  GetList().SetLength(stations.size());
}

void
FrequencyListWidget::OnPaintItem(Canvas &canvas, const PixelRect rc,
                                 unsigned index) noexcept
{
  assert(index < stations.size());
  const auto &station = stations[index];

  char frequency[16];
  station.frequency.Format(frequency, sizeof(frequency));
  StaticString<32> right;
  right.Format("%s MHz", frequency);

  if (two_rows) {
    two_rows_renderer.DrawFirstRow(canvas, rc, station.name.c_str());
    two_rows_renderer.DrawRightFirstRow(canvas, rc, right);
    if (!station.comment.empty())
      two_rows_renderer.DrawSecondRow(canvas, rc, station.comment.c_str());
  } else {
    row_renderer.DrawTextRow(canvas, rc, station.name.c_str());
    row_renderer.DrawRightColumn(canvas, rc, right);
  }
}

void
FrequencyDialogShowModal() noexcept
{
  /* the file is a few dozen lines: read it every time, so an edited
     file is seen without a restart */
  const auto stations =
    LoadFrequencyList(Profile::GetPath(ProfileKeys::FrequenciesFile));

  if (stations.empty()) {
    ShowMessageBox(_("No frequency list is loaded.  Select one under "
                     "System > Site Files > Radio frequencies."),
                   _("Frequency Card"), MB_OK | MB_ICONINFORMATION);
    return;
  }

  const DialogLook &look = UIGlobals::GetDialogLook();

  TWidgetDialog<FrequencyListWidget>
    dialog(WidgetDialog::Full{}, UIGlobals::GetMainWindow(),
           look, _("Frequency Card"));
  dialog.SetWidget(look, stations);
  dialog.GetWidget().CreateButtons(dialog);
  dialog.EnableCursorSelection();
  dialog.ShowModal();
}
