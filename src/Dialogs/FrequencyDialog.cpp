// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Dialogs.h"
#include "Dialogs/WidgetDialog.hpp"
#include "Widget/ListWidget.hpp"
#include "Look/DialogLook.hpp"
#include "UIGlobals.hpp"
#include "Renderer/TextRowRenderer.hpp"
#include "Language/Language.hpp"
#include "ActionInterface.hpp"
#include "Profile/Profile.hpp"
#include "UtilsSettings.hpp"
#include "io/FileLineReader.hpp"
#include "util/StringCompare.hxx"
#include "util/Macros.hpp"

#include "io/KeyValueFileReader.hpp"

#include <string>

class FrequencyListWidget final
  : public ListWidget {

  const DialogLook &dialog_look;
  TextRowRenderer row_renderer;
  Button *active_button, *standby_button, *cancel_button;

public:
  struct RadioChannel {
	  std::string name;
	  RadioFrequency radio_frequency;
  };
  std::vector<RadioChannel> *channels;

public:
  void CreateButtons(WidgetDialog &dialog);

public:
  FrequencyListWidget(const DialogLook &_dialog_look, std::vector<RadioChannel> *_channels)
    :dialog_look(_dialog_look),
	 channels (_channels) {}

  bool UpdateList() noexcept;

  unsigned GetCursorIndex() const {
    return GetList().GetCursorIndex();
  }

public:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;

  /* virtual methods from class List::Handler */
  void OnPaintItem(Canvas &canvas, const PixelRect rc,
                   unsigned index) noexcept override {
    assert(index < channels->size());

    const RadioChannel& channel = (*channels)[index];

    // Draw name and frequency
    row_renderer.DrawTextRow(canvas, rc, channel.name.c_str());

    if (channel.radio_frequency.IsDefined()) {
    	StaticString<30> buffer;
    	char radio[20];
      channel.radio_frequency.Format(radio, ARRAY_SIZE(radio));
      buffer.Format("%s MHz", radio);
      row_renderer.DrawRightColumn(canvas, rc, buffer);
      }

  }

  bool CanActivateItem([[maybe_unused]] unsigned index) const noexcept override {
    return true;
  }

  void OnActivateItem(unsigned index) noexcept override;
};

void
FrequencyListWidget::CreateButtons(WidgetDialog &dialog)
{
  standby_button = dialog.AddButton(_("Set Standby Frequency"), [this](){
    unsigned index = GetCursorIndex();
    assert(index < channels->size());
    const RadioChannel *channel = &(*channels)[index];
    ActionInterface::SetStandbyFrequency(channel->radio_frequency,
            channel->name.c_str());
    cancel_button->Click();
});

  active_button = dialog.AddButton(_("Set Active Frequency"), [this](){
    unsigned index = GetCursorIndex();
    assert(index < channels->size());
    const RadioChannel *channel = &(*channels)[index];
    ActionInterface::SetActiveFrequency(channel->radio_frequency,
            channel->name.c_str());
    cancel_button->Click();
  });

  cancel_button = dialog.AddButton(_("Close"), mrCancel);
}

void
FrequencyListWidget::Prepare(ContainerWindow &parent,
                              const PixelRect &rc) noexcept
{
  CreateList(parent, dialog_look, rc,
             row_renderer.CalculateLayout(*dialog_look.list.font_bold));

  GetList().SetLength(channels->size());
}

void
FrequencyListWidget::OnActivateItem([[maybe_unused]] unsigned index) noexcept
{
  // May still be talking on active frequency
  standby_button->Click();
}

bool
FrequencyListWidget::UpdateList() noexcept
{
  channels->clear();

  auto path = Profile::GetPath(ProfileKeys::FrequenciesFile);
  if (path == nullptr) {
    return false;
  }

  FileLineReaderA reader(path);
  KeyValueFileReader freq_reader(reader);
  KeyValuePair pair;
  while (freq_reader.Read(pair, ':')) {
    RadioChannel *channel = new RadioChannel();
    channel->name = pair.key;
    channel->radio_frequency = RadioFrequency::Parse(pair.value);
    channels->push_back(*channel);
  }
  return !channels->empty();
}

void
FrequencyDialogShowModal() noexcept
{
  static std::vector<FrequencyListWidget::RadioChannel> channels;

  const DialogLook &look = UIGlobals::GetDialogLook();

  auto widget = std::make_unique<FrequencyListWidget>(look,&channels);

  if (channels.size() == 0 || FrequenciesFileChanged == true) {
    FrequenciesFileChanged = false;
	if (widget->UpdateList() == false) {
	  // no channels: don't show the dialog
	  return;
	}
  }

  TWidgetDialog<FrequencyListWidget>
    dialog(WidgetDialog::Full{}, UIGlobals::GetMainWindow(),
           look, _("Frequency Card"));
  widget->CreateButtons(dialog);

  dialog.FinishPreliminary(std::move(widget));
  dialog.EnableCursorSelection();
  dialog.ShowModal();

}
