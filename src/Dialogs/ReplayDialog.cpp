// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ReplayDialog.hpp"
#include "Dialogs/Error.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Widget/RowFormWidget.hpp"
#include "UIGlobals.hpp"
#include "Components.hpp"
#include "BackendComponents.hpp"
#include "CalculationThread.hpp"
#include "MergeThread.hpp"
#include "Protection.hpp"
#include "Replay/Replay.hpp"
#include "Form/DataField/Base.hpp"
#include "Language/Language.hpp"
#include "Repository/FileType.hpp"
#include "system/Path.hpp"
#include "Form/DataField/File.hpp"
#include "Form/Button.hpp"
#include "Form/ButtonPanel.hpp"
#include "Widget/ButtonPanelWidget.hpp"
#include "Renderer/ButtonRenderer.hpp"
#include "Renderer/SymbolRenderer.hpp"
#include "Look/ButtonLook.hpp"
#include "Look/DialogLook.hpp"
#include "ui/canvas/Canvas.hpp"
#include "ui/event/PeriodicTimer.hpp"
#include "time/BrokenTime.hpp"
#include "util/StaticString.hxx"

#include <algorithm>
#include <chrono>
#include <memory>

/**
 * The play/pause toggle: it renders the pause bars while the replay
 * is playing, the play triangle while it is paused or off.
 */
class PlayPauseButtonRenderer final : public ButtonRenderer {
  ButtonFrameRenderer frame_renderer;
  const Replay &replay;

public:
  PlayPauseButtonRenderer(const ButtonLook &_look,
                          const Replay &_replay) noexcept
    :frame_renderer(_look), replay(_replay) {}

  void DrawButton(Canvas &canvas, const PixelRect &rc,
                  ButtonState state) const noexcept override {
    frame_renderer.DrawButton(canvas, rc, state);

    const ButtonLook &look = frame_renderer.GetLook();

    canvas.SelectNullPen();

    switch (state) {
    case ButtonState::DISABLED:
      canvas.Select(look.disabled.brush);
      break;

    case ButtonState::FOCUSED:
    case ButtonState::PRESSED:
      canvas.Select(look.focused.foreground_brush);
      break;

    case ButtonState::SELECTED:
      canvas.Select(look.selected.foreground_brush);
      break;

    case ButtonState::ENABLED:
      canvas.Select(look.standard.foreground_brush);
      break;
    }

    const PixelRect draw_rc = frame_renderer.GetDrawingRect(rc, state);
    if (replay.IsActive() && !replay.IsPaused())
      SymbolRenderer::DrawMedia(canvas, draw_rc,
                                SymbolRenderer::MediaSymbol::PAUSE);
    else
      SymbolRenderer::DrawArrow(canvas, draw_rc, SymbolRenderer::RIGHT);
  }
};

class ReplayControlWidget final
  : public RowFormWidget
{
  enum Controls {
    FILE,
    RATE,
    STATUS,
  };

  Replay &replay;

  ButtonPanelWidget *buttons_widget = nullptr;
  Button *play_button = nullptr;

  UI::PeriodicTimer status_timer{[this]{ UpdateStatus(); }};

public:
  ReplayControlWidget(Replay &_replay, const DialogLook &look) noexcept
    :RowFormWidget(look), replay(_replay) {}

  void SetButtonPanel(ButtonPanelWidget &_buttons) noexcept {
    buttons_widget = &_buttons;
  }

  /* virtual methods from class Widget */
  void Show(const PixelRect &rc) noexcept override {
    RowFormWidget::Show(rc);
    UpdateStatus();
    status_timer.Schedule(std::chrono::milliseconds(500));
  }

  void Hide() noexcept override {
    status_timer.Cancel();
    RowFormWidget::Hide();
  }

private:
  /**
   * The status row below the settings: the current fix number, its
   * UTC time and - when the input can tell - how much of the file
   * has been played.
   */
  void UpdateStatus() noexcept {
    StaticString<64> text;

    if (!replay.IsActive()) {
      text = "-";
    } else {
      text.Format("#%u", replay.GetFixCount());

      const TimeStamp t = replay.GetVirtualTime();
      if (t.IsDefined()) {
        const unsigned second_of_day =
          unsigned(std::chrono::duration_cast<std::chrono::seconds>(t.ToDuration()).count())
          % 86400u;
        const BrokenTime bt = BrokenTime::FromSecondOfDayChecked(second_of_day);
        text.AppendFormat("  %02u:%02u:%02u UTC",
                          bt.hour, bt.minute, bt.second);
      }

      const double progress = replay.GetProgress();
      if (progress >= 0)
        text.AppendFormat("  (%u %%)",
                          unsigned(std::clamp(progress, 0., 1.) * 100));

      if (replay.IsPaused())
        text.AppendFormat("  %s", _("paused"));
    }

    SetText(STATUS, text.c_str());

    /* the play/pause button follows the engine state */
    if (play_button != nullptr)
      play_button->Invalidate();
  }

  /** the tape deck, one row, left to right */
  void CreateTapeButtons(ButtonPanel &panel) noexcept {
    panel.AddSymbol("|<", [this](){ OnResetClicked(); });
    panel.AddSymbol("T/O", [this](){ OnTakeoffClicked(); });
    panel.AddSymbol("<<", [this](){ OnRewindClicked(); });
    play_button =
      panel.Add(std::make_unique<PlayPauseButtonRenderer>(GetLook().button,
                                                          replay),
                [this](){ OnPlayPauseClicked(); });
    panel.AddSymbol(">>", [this](){ OnFastForwardClicked(); });
    panel.AddSymbol(">|", [this](){ OnEndClicked(); });
  }

private:
  bool StartFromFile() noexcept;
  void OnPlayPauseClicked() noexcept;
  void OnResetClicked() noexcept;
  void OnTakeoffClicked() noexcept;
  void OnRewindClicked() noexcept;
  void OnFastForwardClicked() noexcept;
  void OnEndClicked() noexcept;

public:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override;
};

void
ReplayControlWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                             [[maybe_unused]] const PixelRect &rc) noexcept
{
  AddFile(_("File"),
          _("Name of file to replay. May be an IGC file (.igc), a raw NMEA log file (.nmea) or a flight sensor log (.lrsx). Leave blank to run the demo."),
          {},
          {FileType::NMEA, FileType::IGC, FileType::SENSORLOG},
          true);
  LoadValue(FILE, replay.GetFilename());
  GetFileDataField(FILE).Sort(FileDataField::SortOrder::DESCENDING, true);

  AddFloat(_("Rate"),
           _("Time acceleration of replay. Set to 0 for pause, 1 for normal real-time replay."),
           "%.0f x", "%.0f",
           0, 10, 1, false, replay.GetTimeScale());
  GetDataField(RATE).SetOnModified([this]{
    replay.SetTimeScale(GetValueFloat(RATE));
  });

  AddReadOnly(_("Position"),
              _("The current fix number, its UTC time, and how much of the file has been played.  "
                "The buttons work like a tape deck: |< back to the start, "
                "T/O jump to the takeoff, << jump back 10 minutes, "
                "play/pause, >> forward 10 minutes, >| play the rest at "
                "once - the whole flight appears in the trail and the "
                "statistics.  |<, T/O and << jump only: paused stays "
                "paused.  \"Hide\" leaves the replay running; \"Cancel\" "
                "ends it."),
              "-");

  CreateTapeButtons(buttons_widget->GetButtonPanel());
}

inline bool
ReplayControlWidget::StartFromFile() noexcept
{
  const Path path = GetValueFile(FILE);

  try {
    replay.Start(path);
  } catch (...) {
    ShowError(std::current_exception(), _("Replay"));
    return false;
  }

  return true;
}

inline void
ReplayControlWidget::OnPlayPauseClicked() noexcept
{
  if (replay.IsActive() && !replay.IsPaused()) {
    /* pause; another press continues from here */
    replay.SetTimeScale(0);
    UpdateStatus();
    return;
  }

  if (!replay.IsActive()) {
    if (!StartFromFile())
      return;
  }

  double rate = GetValueFloat(RATE);
  if (rate <= 0) {
    rate = 1;
    LoadValue(RATE, rate);
  }
  replay.SetTimeScale(rate);
  UpdateStatus();
}

/**
 * Rewind to the very beginning of the file, keeping the play/pause
 * state: a running replay starts over playing, a paused (or not yet
 * started) one lands paused on the first fix.
 */
inline void
ReplayControlWidget::OnResetClicked() noexcept
{
  const bool was_playing = replay.IsActive() && !replay.IsPaused();

  if (replay.IsActive()) {
    const AllocatedPath path{replay.GetFilename()};
    try {
      replay.Start(path);
    } catch (...) {
      ShowError(std::current_exception(), _("Replay"));
      return;
    }
  } else if (!StartFromFile())
    return;

  if (!was_playing)
    replay.SetTimeScale(0);

  UpdateStatus();
}

/**
 * Jump to the takeoff point - also backwards, by rewinding the file
 * first.  Like "Reset" this only positions the tape: it keeps
 * playing when it was playing, otherwise it waits there for play.
 */
inline void
ReplayControlWidget::OnTakeoffClicked() noexcept
{
  const bool was_playing = replay.IsActive() && !replay.IsPaused();

  if (replay.IsActive()) {
    /* rewind, so the takeoff is found even when the replay is
       already beyond it */
    const AllocatedPath path{replay.GetFilename()};
    try {
      replay.Start(path);
    } catch (...) {
      ShowError(std::current_exception(), _("Replay"));
      return;
    }
  } else if (!StartFromFile())
    return;

  if (!was_playing)
    replay.SetTimeScale(0);

  replay.SeekTakeoff();
  UpdateStatus();
}

inline void
ReplayControlWidget::OnRewindClicked() noexcept
{
  if (!replay.IsActive())
    return;

  const TimeStamp t = replay.GetVirtualTime();
  if (!t.IsDefined())
    return;

  replay.SeekTo(t - FloatDuration{std::chrono::minutes{10}});
  UpdateStatus();
}

inline void
ReplayControlWidget::OnFastForwardClicked() noexcept
{
  replay.FastForward(std::chrono::minutes{10});
  UpdateStatus();
}

/**
 * ">|": play the rest of the file through the computers at once, so
 * the whole flight lands in the trail and the statistics (barogram,
 * distances, speeds), then hold at the landing.
 */
inline void
ReplayControlWidget::OnEndClicked() noexcept
{
  /* one processed fix per ten seconds of flight: coarse enough to
     rush through a long log, fine enough for barogram and totals -
     this is a simulation shortcut, not real life */
  constexpr FloatDuration end_interval = std::chrono::seconds{10};

  if (!replay.IsActive())
    return;

  auto *merge_thread = backend_components->merge_thread.get();
  auto *calc_thread = backend_components->calculation_thread.get();
  if (merge_thread == nullptr || calc_thread == nullptr)
    return;

  merge_thread->Suspend();

  {
    const ScopeSuspendAllThreads suspend;
    replay.ProcessAllFixes(*merge_thread, *calc_thread, end_interval);
  }

  merge_thread->Resume();

  TriggerCalculatedUpdate();
  TriggerMapUpdate();

  /* hold at the landing; "|<" or "T/O" start over */
  replay.SetTimeScale(0);
  UpdateStatus();
}

void
ShowReplayDialog(Replay &replay) noexcept
{
  const DialogLook &look = UIGlobals::GetDialogLook();

  auto control = std::make_unique<ReplayControlWidget>(replay, look);
  auto *panel =
    new ButtonPanelWidget(std::move(control),
                          ButtonPanelWidget::Alignment::BOTTOM);
  ((ReplayControlWidget &)panel->GetWidget()).SetButtonPanel(*panel);

  WidgetDialog dialog(WidgetDialog::Auto{}, UIGlobals::GetMainWindow(),
                      look, _("Replay"), panel);

  /* "Hide" only closes the window - the replay keeps running */
  dialog.AddButton(_("Hide"), mrOK);

  /* "Cancel" ends the replay altogether */
  dialog.AddButton(_("Cancel"), [&replay, &dialog](){
    replay.Stop();
    dialog.SetModalResult(mrCancel);
  });

  dialog.ShowModal();
}
