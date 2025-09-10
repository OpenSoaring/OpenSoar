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

#include "UIGlobals.hpp"
#include "Interface.hpp"
#include "ui/window/TopWindow.hpp"
#include "MainWindow.hpp"

#include "Weather/Skysight/Skysight.hpp"

#include <string_view>

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
SkysightListItemRenderer::Draw(Canvas &canvas, const PixelRect rc, unsigned index) {
  const ComputerSettings &settings = CommonInterface::GetComputerSettings();
  SkySight::Layer *layer = skysight->GetSelectedLayer(index);

  std::string first_row = std::string(layer->name);
  if (skysight->GetActiveLayerId() == layer->id)
    first_row += " [ACTIVE]";

  StaticString<256> second_row;

  if (layer->live_layer || layer->tile_layer ) {
    second_row.Format("%s", _("Not a Forecast Layer!"));
  } else if (layer->updating) {
    second_row.Format("%s", _("Updating..."));
  } else {
    if (!layer->from || !layer->to || !layer->mtime) {
      second_row.Format("%s", _("No data. Press \"Update\" to update."));
    } else {
      uint64_t elapsed = std::chrono::system_clock::to_time_t(
        BrokenDateTime::NowUTC().ToTimePoint()) - layer->mtime;

      second_row.Format(_("Data from %s to %s. Updated %s ago"), 
        FormatLocalTimeHHMM(
          TimeStamp(std::chrono::duration<double>(layer->from)),
          settings.utc_offset).c_str(),
        FormatLocalTimeHHMM(
          TimeStamp(std::chrono::duration<double>(layer->to)),
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
    auto layer = skysight->GetLayer(i);
    if (layer)
      row_renderer.DrawTextRow(canvas, rc, layer->name.c_str());
  }
  
  static const char* HelpCallback(unsigned i)
  {
    std::shared_ptr<Skysight> skysight = DataGlobals::GetSkysight();

    if (!skysight)
      return _("No description available.");

    auto layer = skysight->GetLayer(i);
    if (layer) {
      std::string_view helptext = layer->desc;
      char *help = new char[helptext.length() + 1];
      std::strcpy(help, helptext.data());
      return help;
    } else {
      return nullptr;
    }
  }  
};

class SkysightWidget final
  : public ListWidget
{
  ButtonPanelWidget *buttons_widget;

  Button *activate_button;
  Button *add_button;
  Button *remove_button;
#ifdef SKYSIGHT_OFFLINE_MODE
  Button *update_button;
  Button *updateall_button;
#endif  // SKYSIGHT_OFFLINE_MODE
  Button *close_button;
  // Button *cancel_button;

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
#ifdef SKYSIGHT_OFFLINE_MODE
  void UpdateClicked();
  void UpdateAllClicked();
#endif
  void RemoveClicked();
  void CancelClicked();
  void CloseClicked();

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
  bool CanActivateItem([[maybe_unused]] unsigned index) const noexcept override {
    return true;
  }

  void OnActivateItem([[maybe_unused]] unsigned index) noexcept override {};

  Button::Callback activate_callback = [this]() { SkysightWidget::ActivateClicked(); };
  Button::Callback deactivate_callback = [this]() { SkysightWidget::DeactivateClicked(); };

private:

};

void
SkysightWidget::CreateButtons(ButtonPanel &buttons)
{
  activate_button = buttons.Add(_("Activate"), [this](){ ActivateClicked(); });
  add_button = buttons.Add(_("Add"), [this](){ AddClicked(); });
  remove_button = buttons.Add(_("Remove"), [this](){ RemoveClicked(); });
#ifdef SKYSIGHT_OFFLINE_MODE
  update_button = buttons.Add(_("Update"), [this](){ UpdateClicked(); });
  updateall_button = buttons.Add(_("Update All"), [this](){ UpdateAllClicked(); });
#endif
  //  cancel_button = buttons.Add(_("Cancel"), [this](){ CancelClicked(); });
  close_button = buttons.Add(_("Close"), [this](){ CloseClicked(); });
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
  size_t index = GetList().GetCursorIndex();
  bool item_updating = false;
  bool item_active = false;

  if (index < skysight->NumSelectedLayers()) {
    auto layer = skysight->GetSelectedLayer(index);
    item_updating = layer->updating;
    if (skysight->GetActiveLayer())
        item_active = (skysight->GetActiveLayer()->id == layer->id);
  }

  ListControl &list = GetList();
  list.SetLength(skysight->NumSelectedLayers());
  list.Invalidate();

  add_button->SetEnabled(!skysight->SelectedLayersFull());
  remove_button->SetEnabled(!item_updating);

  if (list.GetLength() == 0) {
    activate_button->SetEnabled(false);
    activate_button->SetCaption(_("No Item"));
    activate_button->SetCallback(nullptr);
  } else if(item_active) {
    activate_button->SetEnabled(true);
    activate_button->SetCaption(_("Deactivate"));
    activate_button->SetCallback(deactivate_callback); //  [this]() { DeactivateClicked(); });
  } else {
    activate_button->SetEnabled(true);
    activate_button->SetCaption(_("Activate"));
    activate_button->SetCallback(activate_callback);  //  [this]() { ActivateClicked(); });
  }
}

void
SkysightWidget::OnPaintItem(Canvas &canvas, const PixelRect rc, unsigned index) noexcept
{
  row_renderer.Draw(canvas, rc, index);
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
  auto layer = skysight->GetLayer(i);
  if (layer)
    skysight->AddSelectedLayer(layer->id.c_str());

  UpdateList();
}

#ifdef SKYSIGHT_OFFLINE_MODE
void SkysightWidget::UpdateClicked()
{
  unsigned index = GetList().GetCursorIndex();
  assert(index < (unsigned)skysight->NumSelectedLayers());

  auto layer = skysight->GetSelectedLayer(index);
  // skysight->GetLayer(index);
  if (!skysight->DownloadSelectedLayer(layer->id))
    ShowMessageBox(_("Couldn't update data."), _("Update Error"), MB_OK);

  UpdateList();
}


void SkysightWidget::UpdateAllClicked()
{
  if (!skysight->DownloadSelectedLayer("*"))
    ShowMessageBox(_("Couldn't update data."), _("Update Error"), MB_OK);
  UpdateList();
}
#endif // SKYSIGHT_OFFLINE_MODE

void SkysightWidget::RemoveClicked()
{
  unsigned index = GetList().GetCursorIndex();
  assert(index < (unsigned)skysight->NumSelectedLayers());

  SkySight::Layer *layer = skysight->GetSelectedLayer(index);
  StaticString<256> tmp;
  tmp.Format(_("Do you want to remove \"%s\"?"),
             layer->name.c_str());

  if (ShowMessageBox(tmp, _("Remove"), MB_YESNO) == IDNO)
    return;

  skysight->RemoveSelectedLayer(index);

  UpdateList();
}

inline void
SkysightWidget::ActivateClicked()
{
  unsigned index = GetList().GetCursorIndex();
  assert(index < (unsigned)skysight->NumSelectedLayers());
  if (index < (unsigned)skysight->NumSelectedLayers()) {
    SkySight::Layer *layer = skysight->GetSelectedLayer(index);
    if (!skysight->SetLayerActive(layer->id))
      ShowMessageBox(_("Couldn't display data."),
        _("Display Error"), MB_OK);
    UpdateList();
  }
}

inline void
SkysightWidget::DeactivateClicked()
{
  skysight->DeactivateLayer();
  UpdateList();
}

#if 0
inline void
SkysightWidget::CancelClicked()
{
  // remove changes...
  // t.b.d.!
  CommonInterface::main_window->Close();
}
#endif

inline void
SkysightWidget::CloseClicked()
{
  // t.b.d.: Save the settings!
  CommonInterface::main_window->Close();
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
