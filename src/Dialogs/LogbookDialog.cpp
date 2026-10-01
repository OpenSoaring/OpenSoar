// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "LogbookDialog.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Dialogs/Message.hpp"
#include "Dialogs/Error.hpp"
#include "Widget/ListWidget.hpp"
#include "Widget/RowFormWidget.hpp"
#include "Form/Button.hpp"
#include "Form/DataField/Enum.hpp"
#include "Renderer/TwoTextRowsRenderer.hpp"
#include "Logger/Logbook.hpp"
#include "DataFilePath.hpp"
#include "system/Path.hpp"
#include "Formatter/UserUnits.hpp"
#include "Interface.hpp"
#include "UIGlobals.hpp"
#include "Look/DialogLook.hpp"
#include "Language/Language.hpp"
#include "util/StaticString.hxx"

#include <fmt/format.h>

#include <algorithm>
#include <cassert>
#include <vector>

/* this macro exists in the WIN32 API */
#ifdef DELETE
#undef DELETE
#endif

/**
 * The pilot reads the times of the log book in local time; the file
 * keeps UTC.
 */
[[gnu::pure]]
static BrokenDateTime
ToLocal(const BrokenDateTime &utc) noexcept
{
  if (!utc.IsPlausible())
    return utc;

  return utc + CommonInterface::GetComputerSettings().utc_offset.ToDuration();
}

static StaticString<16>
FormatHHMM(const BrokenDateTime &utc) noexcept
{
  StaticString<16> buffer;
  if (utc.IsPlausible()) {
    const auto local = ToLocal(utc);
    buffer.Format("%02u:%02u", local.hour, local.minute);
  } else
    buffer = "--:--";
  return buffer;
}

static StaticString<16>
FormatDate(const BrokenDateTime &utc) noexcept
{
  const auto local = ToLocal(utc);
  StaticString<16> buffer;
  buffer.Format("%04u-%02u-%02u", local.year, local.month, local.day);
  return buffer;
}

static StaticString<16>
FormatFlightTime(const LogbookEntry &entry) noexcept
{
  StaticString<16> buffer;
  if (entry.HasFlightTime()) {
    const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(entry.GetFlightTime()).count();
    buffer.Format("%u:%02u", unsigned(minutes / 60), unsigned(minutes % 60));
  } else
    buffer.clear();
  return buffer;
}

static const char *
GetShapeName(LogbookStatistics::DMStShape shape) noexcept
{
  switch (shape) {
  case LogbookStatistics::DMStShape::NONE:
    break;
  case LogbookStatistics::DMStShape::QUADRILATERAL:
    return _("Free distance");
  case LogbookStatistics::DMStShape::TRIANGLE:
    return _("Triangle");
  case LogbookStatistics::DMStShape::OUT_AND_RETURN:
    return _("Out and return");
  }

  return "";
}

/**
 * The details of one flight; the values the computer cannot know
 * (launch, crew, remark) can be edited.
 */
class LogbookEntryWidget final : public RowFormWidget {
  enum Rows {
    DATE,
    TAKEOFF,
    LANDING,
    DURATION,
    AIRCRAFT,
    FREE,
    DMST,
    MAX_ALTITUDE,
    IGC,
    LAUNCH,
    PILOT,
    COPILOT,
    REMARK,
  };

  LogbookEntry &entry;

public:
  LogbookEntryWidget(const DialogLook &look, LogbookEntry &_entry) noexcept
    :RowFormWidget(look), entry(_entry) {}

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;
};

void
LogbookEntryWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                            [[maybe_unused]] const PixelRect &rc) noexcept
{
  AddReadOnly(_("Date"), nullptr, FormatDate(entry.takeoff));

  StaticString<128> text;
  text.Format("%s  %s", FormatHHMM(entry.takeoff).c_str(),
              entry.takeoff_place.c_str());
  AddReadOnly(C_("Logbook", "Takeoff"), nullptr, text);

  text.Format("%s  %s", FormatHHMM(entry.landing).c_str(),
              entry.landing_place.c_str());
  AddReadOnly(C_("Logbook", "Landing"), nullptr, text);

  AddReadOnly(_("Flight time"), nullptr, FormatFlightTime(entry));

  text.Format("%s  %s  %s", entry.aircraft.c_str(),
              entry.registration.c_str(), entry.competition_id.c_str());
  AddReadOnly(_("Aircraft"), nullptr, text);

  AddReadOnly(_("Free distance"),
              _("The free distance over up to five turn points (six legs), "
                "as the OLC classic rules score it."),
              entry.free_distance > 0
              ? FormatUserDistance(entry.free_distance, true, 1).c_str()
              : "");

  if (entry.dmst_distance > 0)
    text.Format("%s  %.1f %s  %s",
                FormatUserDistance(entry.dmst_distance, true, 1).c_str(),
                entry.dmst_points, _("pts"),
                GetShapeName(entry.dmst_shape));
  else
    text.clear();
  AddReadOnly("DMSt",
              _("Distance and points by the DMSt rules, with the index of "
                "the plane and the bonus of the shape."),
              text);

  if (entry.max_altitude >= 0)
    AddReadOnly(_("Max. altitude"), nullptr,
                FormatUserAltitude(entry.max_altitude).c_str());
  else
    AddReadOnly(_("Max. altitude"), nullptr, "");

  AddReadOnly(_("IGC file"), nullptr, entry.igc_file.c_str());

  static constexpr StaticEnumChoice launch_list[] = {
    { LogbookEntry::Launch::UNKNOWN, N_("Unknown") },
    { LogbookEntry::Launch::WINCH, N_("Winch") },
    { LogbookEntry::Launch::AEROTOW, N_("Aerotow") },
    { LogbookEntry::Launch::SELF, N_("Self-launch") },
    nullptr
  };
  AddEnum(_("Launch"),
          _("Estimated from the climb after the takeoff; correct it if "
            "it is wrong."),
          launch_list, unsigned(entry.launch));

  AddText(_("Pilot"), nullptr, entry.pilot.c_str());
  AddText(_("Co-pilot"), nullptr, entry.copilot.c_str());
  AddText(_("Remark"), nullptr, entry.remark.c_str());
}

bool
LogbookEntryWidget::Save(bool &_changed) noexcept
{
  bool changed = false;

  changed |= SaveValueEnum(LAUNCH, entry.launch);
  changed |= SaveValue(PILOT, entry.pilot);
  changed |= SaveValue(COPILOT, entry.copilot);
  changed |= SaveValue(REMARK, entry.remark);

  _changed |= changed;
  return true;
}

/**
 * The list of all flights, newest first.
 */
class LogbookListWidget final : public ListWidget {
  const AllocatedPath path;

  /** oldest first, as in the file */
  std::vector<LogbookEntry> entries;

  TwoTextRowsRenderer row_renderer;

  WidgetDialog *form = nullptr;
  Button *details_button = nullptr, *delete_button = nullptr;

public:
  LogbookListWidget() noexcept
    :path(LogsDataSavePath("logbook.csv")) {}

  void CreateButtons(WidgetDialog &dialog) noexcept;

private:
  [[gnu::pure]]
  unsigned ToEntryIndex(unsigned list_index) const noexcept {
    assert(list_index < entries.size());
    return entries.size() - 1 - list_index;
  }

  void Load() noexcept;
  bool Store() noexcept;
  void UpdateButtons() noexcept;

  void ShowDetails(unsigned list_index) noexcept;
  void DeleteClicked() noexcept;

public:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;

protected:
  /* virtual methods from ListItemRenderer */
  void OnPaintItem(Canvas &canvas, PixelRect rc,
                   unsigned idx) noexcept override;

  /* virtual methods from ListCursorHandler */
  bool CanActivateItem([[maybe_unused]] unsigned index) const noexcept override {
    return true;
  }

  void OnActivateItem(unsigned index) noexcept override {
    ShowDetails(index);
  }
};

void
LogbookListWidget::Load() noexcept
{
  try {
    entries = Logbook::Read(path);

    /* the file is in the order the flights were recorded; an entry
       added by hand may be out of place, the list shows them by time */
    std::stable_sort(entries.begin(), entries.end(),
                     [](const LogbookEntry &a, const LogbookEntry &b){
                       return a.takeoff < b.takeoff;
                     });
  } catch (...) {
    ShowError(std::current_exception(), _("Log book"));
    entries.clear();
  }

  GetList().SetLength(entries.size());
  GetList().Invalidate();
  UpdateButtons();
}

bool
LogbookListWidget::Store() noexcept
{
  try {
    Logbook::Write(path, entries);
    return true;
  } catch (...) {
    ShowError(std::current_exception(), _("Failed to save file."));
    return false;
  }
}

void
LogbookListWidget::UpdateButtons() noexcept
{
  const bool empty = entries.empty();
  if (details_button != nullptr)
    details_button->SetEnabled(!empty);
  if (delete_button != nullptr)
    delete_button->SetEnabled(!empty);
  if (form != nullptr)
    form->ResyncButtonPanelSelection();
}

void
LogbookListWidget::CreateButtons(WidgetDialog &dialog) noexcept
{
  form = &dialog;
  details_button = dialog.AddButton(_("Details"), [this](){
    ShowDetails(GetList().GetCursorIndex());
  });
  delete_button = dialog.AddButton(C_("Button", "Delete"),
                                   [this](){ DeleteClicked(); });
  UpdateButtons();
}

void
LogbookListWidget::Prepare(ContainerWindow &parent,
                           const PixelRect &rc) noexcept
{
  const DialogLook &look = UIGlobals::GetDialogLook();
  CreateList(parent, look, rc,
             row_renderer.CalculateLayout(*look.list.font_bold,
                                          look.small_font));
  Load();
}

void
LogbookListWidget::OnPaintItem(Canvas &canvas, const PixelRect rc,
                               unsigned list_index) noexcept
{
  const LogbookEntry &e = entries[ToEntryIndex(list_index)];

  StaticString<256> text;
  text.Format("%s  %s-%s  %s   %s %s",
              FormatDate(e.takeoff).c_str(),
              FormatHHMM(e.takeoff).c_str(), FormatHHMM(e.landing).c_str(),
              FormatFlightTime(e).c_str(),
              e.aircraft.c_str(), e.registration.c_str());
  row_renderer.DrawFirstRow(canvas, rc, text);

  if (e.takeoff_place == e.landing_place || e.landing_place.empty())
    text = e.takeoff_place.c_str();
  else
    text.Format("%s > %s", e.takeoff_place.c_str(), e.landing_place.c_str());

  if (e.free_distance > 0)
    text.AppendFormat("   %s", FormatUserDistance(e.free_distance).c_str());

  if (e.dmst_points > 0)
    text.AppendFormat("   DMSt %.0f %s", e.dmst_points, _("pts"));

  row_renderer.DrawSecondRow(canvas, rc, text);
}

void
LogbookListWidget::ShowDetails(unsigned list_index) noexcept
{
  if (list_index >= entries.size())
    return;

  LogbookEntry &entry = entries[ToEntryIndex(list_index)];
  LogbookEntry edited = entry;

  const DialogLook &look = UIGlobals::GetDialogLook();
  TWidgetDialog<LogbookEntryWidget>
    dialog(WidgetDialog::Auto{}, UIGlobals::GetMainWindow(), look,
           _("Flight"));
  dialog.SetWidget(look, edited);
  dialog.AddButton(_("OK"), mrOK);
  dialog.AddButton(_("Cancel"), mrCancel);

  /* on OK, the dialog has saved the widget into "edited" already */
  if (dialog.ShowModal() != mrOK || !dialog.GetChanged())
    return;

  entry = std::move(edited);
  Store();
  GetList().Invalidate();
}

void
LogbookListWidget::DeleteClicked() noexcept
{
  const unsigned list_index = GetList().GetCursorIndex();
  if (list_index >= entries.size())
    return;

  if (ShowMessageBox(_("Delete this flight from the log book?"),
                     C_("Button", "Delete"), MB_YESNO) != IDYES)
    return;

  entries.erase(entries.begin() + ToEntryIndex(list_index));
  Store();

  GetList().SetLength(entries.size());
  GetList().Invalidate();
  UpdateButtons();
}

void
ShowLogbookDialog() noexcept
{
  TWidgetDialog<LogbookListWidget>
    dialog(WidgetDialog::Full{}, UIGlobals::GetMainWindow(),
           UIGlobals::GetDialogLook(), _("Log book"));
  dialog.SetWidget();
  dialog.GetWidget().CreateButtons(dialog);
  dialog.AddButton(_("Close"), mrOK);
  dialog.EnableCursorSelection();
  dialog.ShowModal();
}
