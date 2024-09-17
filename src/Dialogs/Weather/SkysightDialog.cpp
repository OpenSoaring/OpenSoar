// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "SkysightDialog.hpp"
#include "Dialogs/Message.hpp"
#include "Dialogs/ListPicker.hpp"
#include "Dialogs/JobDialog.hpp"
#include "Language/Language.hpp"
#include "Weather/Features.hpp"

#ifdef HAVE_SKYSIGHT
#include "UIGlobals.hpp"
#include "Look/DialogLook.hpp"
#include "Dialogs/TextEntry.hpp"
#include "Form/Button.hpp"
#include "Form/ButtonPanel.hpp"
#include "Widget/ListWidget.hpp"
#include "Widget/ButtonPanelWidget.hpp"
#include "Weather/Skysight/Skysight.hpp"
#include "Operation/VerboseOperationEnvironment.hpp"
#include "util/TrivialArray.hxx"
#include "util/StringAPI.hxx"
#include "Renderer/TextRowRenderer.hpp"
#include "Renderer/TwoTextRowsRenderer.hpp"
#include "ui/event/PeriodicTimer.hpp"

#include "Protection.hpp"
#include "DataGlobals.hpp"
#include "Interface.hpp"

#include "Language/Language.hpp"
#include "LocalPath.hpp"

#include "MapWindow/OverlayBitmap.hpp"
#include "MapWindow/GlueMapWindow.hpp"
#include "Dialogs/Error.hpp"

#include "util/StaticString.hxx"
#include "Language/Language.hpp"
#include "time/BrokenDateTime.hpp"
#include "Formatter/TimeFormatter.hpp"
#include "Formatter/LocalTimeFormatter.hpp"

#include "Weather/Skysight/Skysight.hpp"

class SkysightListItemRenderer
{
  TwoTextRowsRenderer row_renderer; 

public:
  SkysightListItemRenderer(): skysight(DataGlobals::GetSkysight()) {}

  void Draw(Canvas &canvas, const PixelRect rc, unsigned i); 

  unsigned CalculateLayout(const DialogLook &look) {
    return row_renderer.CalculateLayout(*look.list.font_bold, look.small_font);
  }

private:
  std::shared_ptr<Skysight> skysight;
};

//TODO: Could extend this to extract Skysight data at point and display on list (see NOAAListRenderer::Draw(2)
void
SkysightListItemRenderer::Draw(Canvas &canvas, const PixelRect rc, unsigned i) {
  const ComputerSettings &settings = CommonInterface::GetComputerSettings();
  SkysightActiveLayer m = SkysightActiveLayer(skysight->GetActiveLayer(i));

  std::string first_row = std::string(m.layer->name);
  if (skysight->displayed_layer == m.layer->id.c_str())
    first_row += " [ACTIVE]";

  StaticString<256> second_row;

  if (m.updating) {
    second_row.Format("%s", _("Updating..."));
  } else {
    if (!m.from || !m.to || !m.mtime) {
      second_row.Format("%s", _("No data. Press \"Update\" to update."));
    } else {
      uint64_t elapsed = std::chrono::system_clock::to_time_t(
        BrokenDateTime::NowUTC().ToTimePoint()) - m.mtime;

      second_row.Format(_("Data from %s to %s. Updated %s ago"), 
                        FormatLocalTimeHHMM(
                          TimeStamp(std::chrono::duration<double>(m.from)),
                          settings.utc_offset).c_str(),
                        FormatLocalTimeHHMM(
                          TimeStamp(std::chrono::duration<double>(m.to)),
                          settings.utc_offset).c_str(),
                        FormatTimespanSmart(std::chrono::seconds(elapsed)).c_str());  
    }
  }

  row_renderer.DrawFirstRow(canvas, rc, first_row.c_str());
  row_renderer.DrawSecondRow(canvas, rc, second_row.c_str());
}

/**
 * RENDERER FOR METRICS POPUP LIST
 */
class SkysightLayersListItemRenderer: public ListItemRenderer
{
  TextRowRenderer row_renderer;
  std::shared_ptr<Skysight> skysight;

public:
  SkysightLayersListItemRenderer(): skysight(DataGlobals::GetSkysight()) {}
  
  unsigned CalculateLayout(const DialogLook &look) {
    return row_renderer.CalculateLayout(*look.list.font);
  }

  void OnPaintItem(Canvas &canvas, const PixelRect rc, unsigned i) noexcept override {
    row_renderer.DrawTextRow(canvas, rc, skysight->GetLayer(i).name.c_str());
  }
  
  static const char* HelpCallback(unsigned i)
  {
    std::shared_ptr<Skysight> skysight = DataGlobals::GetSkysight();

    if (!skysight)
      return _("No description available.");

    std::string helptext = skysight->GetLayer(i).desc;
    char *help = new char[helptext.length() + 1];
    std::strcpy(help, helptext.c_str());
    return help;
  }  
};

class SkysightWidget final
  : public ListWidget
{
  ButtonPanelWidget *buttons_widget;

  Button *activate_button, *deactivate_button, *add_button, *remove_button,
    *update_button, *updateall_button;

  struct ListItem {
    StaticString<255> name;

    gcc_pure bool operator<(const ListItem &i2) const
    {
      return StringCollate(name, i2.name) < 0;
    }
  };

  SkysightListItemRenderer row_renderer;
  std::shared_ptr<Skysight> skysight;
  UI::PeriodicTimer timer{[this]{ UpdateList(); }};

public:
  explicit SkysightWidget(std::shared_ptr<Skysight> &&_skysight)
    : skysight(std::move(_skysight)) {}

  void SetButtonPanel(ButtonPanelWidget &_buttons) {
    buttons_widget = &_buttons;
  }

  void CreateButtons(ButtonPanel &buttons);

private:
  void UpdateList();
  void ActivateClicked();
  void DeactivateClicked();
  void AddClicked();
  void UpdateClicked();
  void UpdateAllClicked();
  void RemoveClicked();

public:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
                       const PixelRect &rc) noexcept override;
  void Unprepare() noexcept override;

protected:
  /* virtual methods from ListItemRenderer */
  void OnPaintItem(Canvas &canvas, const PixelRect rc,
                           unsigned idx) noexcept override;

  /* virtual methods from ListCursorHandler */
  bool CanActivateItem(__attribute__ ((unused)) unsigned index) const noexcept override {
    return true;
  }

  void OnActivateItem(__attribute__ ((unused)) unsigned index) noexcept override {};

private:

};

void
SkysightWidget::CreateButtons(ButtonPanel &buttons)
{
  activate_button = buttons.Add(_("Activate"), [this](){ ActivateClicked(); });
  deactivate_button = buttons.Add(_("Deactivate"), [this](){ DeactivateClicked(); });
  add_button = buttons.Add(_("Add"), [this](){ AddClicked(); });
  remove_button = buttons.Add(_("Remove"), [this](){ RemoveClicked(); });
  update_button = buttons.Add(_("Update"), [this](){ UpdateClicked(); });
  updateall_button = buttons.Add(_("Update All"), [this](){ UpdateAllClicked(); });
}

void
SkysightWidget::Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept
{
  CreateButtons(buttons_widget->GetButtonPanel());
  const DialogLook &look = UIGlobals::GetDialogLook();
  CreateList(parent, look, rc, row_renderer.CalculateLayout(look));
  UpdateList();
  timer.Schedule(std::chrono::milliseconds(500));
}

void
SkysightWidget::Unprepare() noexcept
{
  timer.Cancel();
  DeleteWindow();
}

void
SkysightWidget::UpdateList()
{
  unsigned index = GetList().GetCursorIndex();
  bool item_updating = false;
  bool item_active = false;

  if ((int)index < skysight->NumActiveLayers()) {
    SkysightActiveLayer a = skysight->GetActiveLayer(index);
    item_updating = a.updating;
    item_active = (skysight->displayed_layer == a.layer->id.c_str());
  }

  bool any_updating = skysight->ActiveLayersUpdating();
  bool empty = (!(bool)skysight->NumActiveLayers());

  ListControl &list = GetList();
  list.SetLength(skysight->NumActiveLayers());
  list.Invalidate();

  add_button->SetEnabled(!skysight->ActiveLayersFull());
  remove_button->SetEnabled(!empty && !item_updating);
  update_button->SetEnabled(!empty && !item_updating);
  updateall_button->SetEnabled(!empty && !any_updating);
  activate_button->SetEnabled(!empty && !item_updating); 
  deactivate_button->SetEnabled(!empty && item_active);
}

void
SkysightWidget::OnPaintItem(Canvas &canvas, const PixelRect rc, unsigned index) noexcept
{
  row_renderer.Draw(canvas, rc, index);
}

void SkysightWidget::AddClicked()
{
  if (!skysight->IsReady()) {
    ShowMessageBox(
      _("Please check your Skysight settings and internet connection."),
      _("Couldn't connect to Skysight"),
      MB_OK
    );
    return;
  }

  SkysightLayersListItemRenderer item_renderer;

  int i = ListPicker(_("Choose a parameter"),
                     skysight->NumLayers(), 0,
                     item_renderer.CalculateLayout(UIGlobals::GetDialogLook()),
                     item_renderer,
                     false, /*timer */
                     nullptr, /*char help text */
                     &SkysightLayersListItemRenderer::HelpCallback,
                     nullptr /*Extra caption*/);

  if (i < 0)
    return;

  assert((int)i < skysight->NumLayers());
  skysight->AddActiveLayer(skysight->GetLayer(i).id.c_str());

  UpdateList();
}

void SkysightWidget::UpdateClicked()
{
  unsigned index = GetList().GetCursorIndex();
  assert(index < (unsigned)skysight->NumActiveLayers());

  SkysightActiveLayer a = skysight->GetActiveLayer(index);  
  if (!skysight->DownloadActiveLayer(a.layer->id))
    ShowMessageBox(_("Couldn't update data."), _("Update Error"), MB_OK);
  UpdateList();
}

void SkysightWidget::UpdateAllClicked()
{
  if (!skysight->DownloadActiveLayer("*"))
    ShowMessageBox(_("Couldn't update data."), _("Update Error"), MB_OK);
  UpdateList();
}

void SkysightWidget::RemoveClicked()
{
  unsigned index = GetList().GetCursorIndex();
  assert(index < (unsigned)skysight->NumActiveLayers());

  SkysightActiveLayer a = skysight->GetActiveLayer(index);
  StaticString<256> tmp;
  tmp.Format(_("Do you want to remove \"%s\"?"),
             a.layer->name.c_str());

  if (ShowMessageBox(tmp, _("Remove"), MB_YESNO) == IDNO)
    return;

  skysight->RemoveActiveLayer(a.layer->id);

  UpdateList();
}

inline void
SkysightWidget::ActivateClicked()
{
  unsigned index = GetList().GetCursorIndex();
  assert(index < (unsigned)skysight->NumActiveLayers());

  SkysightActiveLayer a = skysight->GetActiveLayer(index);  
  if (!skysight->DisplayActiveLayer(a.layer->id.c_str()))
    ShowMessageBox(_("Couldn't display data. There is no forecast data available for this time."),
		   _("Display Error"), MB_OK);
  UpdateList();
}

inline void
SkysightWidget::DeactivateClicked()
{
  unsigned index = GetList().GetCursorIndex();
  assert(index < (unsigned)skysight->NumActiveLayers());

  skysight->DisplayActiveLayer();
  UpdateList();
}

std::unique_ptr<Widget>
CreateSkysightWidget()
{
  auto skysight = DataGlobals::GetSkysight();
  auto buttons =
    std::make_unique<ButtonPanelWidget>(std::make_unique<SkysightWidget>(std::move(skysight)),
                                        ButtonPanelWidget::Alignment::BOTTOM);
  ((SkysightWidget &)buttons->GetWidget()).SetButtonPanel(*buttons);
  return buttons;
}
#endif
