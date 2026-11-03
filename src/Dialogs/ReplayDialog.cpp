// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ReplayDialog.hpp"
#include "Dialogs/DataManagement/ExportFlightsPanel.hpp"
#include "Dialogs/Error.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Widget/RowFormWidget.hpp"
#include "Widget/ListWidget.hpp"
#include "Renderer/TwoTextRowsRenderer.hpp"
#include "UIGlobals.hpp"
#include "Interface.hpp"
#include "Components.hpp"
#include "BackendComponents.hpp"
#include "CalculationThread.hpp"
#include "MergeThread.hpp"
#include "Replay/Replay.hpp"
#include "Profile/Profile.hpp"
#include "Profile/Keys.hpp"
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
#include "ui/canvas/Icon.hpp"
#include "ui/canvas/Brush.hpp"
#include "Resources.hpp"
#include "ui/control/ProgressBar.hpp"
#include "Screen/Layout.hpp"
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

/**
 * A button of the jump row: an arrow and the symbol of the flight
 * phase it jumps to, the circling symbol for a thermal and the glide
 * symbol for a glide.  The arrow points at the symbol - "-> O" jumps
 * forward to the next one, "O <-" back to the previous one.
 */
class JumpButtonRenderer final : public ButtonRenderer {
public:
  enum class Kind { THERMAL, GLIDE };

private:
  ButtonFrameRenderer frame_renderer;
  MaskedIcon icon;
  const bool forward;

public:
  JumpButtonRenderer(const ButtonLook &_look, Kind kind,
                     bool _forward) noexcept
    :frame_renderer(_look), forward(_forward) {
    /* the ..._ALL macros expand to an argument list */
    if (kind == Kind::THERMAL)
      icon.LoadResource(IDB_CLIMB_ALL);
    else
      icon.LoadResource(IDB_CRUISE_ALL);
  }

  void DrawButton(Canvas &canvas, const PixelRect &rc,
                  ButtonState state) const noexcept override {
    frame_renderer.DrawButton(canvas, rc, state);

    const ButtonLook &look = frame_renderer.GetLook();

    Color color;
    switch (state) {
    case ButtonState::DISABLED:
      color = look.disabled.color;
      break;

    case ButtonState::FOCUSED:
    case ButtonState::PRESSED:
      color = look.focused.foreground_color;
      break;

    case ButtonState::SELECTED:
      color = look.selected.foreground_color;
      break;

    case ButtonState::ENABLED:
    default:
      color = look.standard.foreground_color;
      break;
    }

    const PixelRect draw_rc = frame_renderer.GetDrawingRect(rc, state);
    const int height = int(draw_rc.GetHeight());
    if (height <= 4)
      return;

    /* symbol and arrow side by side, centred together */
    const int symbol_size = std::max(4, height * 4 / 5);
    const int arrow_length = std::max(4, height * 3 / 5);
    const int gap = std::max(2, symbol_size / 5);
    const int total = symbol_size + gap + arrow_length;

    const auto center = draw_rc.GetCenter();
    const int left = center.x - total / 2;

    int arrow_left, symbol_center_x;
    if (forward) {
      arrow_left = left;
      symbol_center_x = left + arrow_length + gap + symbol_size / 2;
    } else {
      symbol_center_x = left + symbol_size / 2;
      arrow_left = left + symbol_size + gap;
    }

    DrawArrow(canvas, arrow_left, arrow_left + arrow_length, center.y,
              forward, color);

    if (icon.IsDefined())
      icon.Draw(canvas, {symbol_center_x, center.y}, unsigned(symbol_size));
  }

private:
  /**
   * A real arrow with shaft and head between \a x1 and \a x2, the head
   * at the right end if \a right.
   */
  static void DrawArrow(Canvas &canvas, int x1, int x2, int y, bool right,
                        Color color) noexcept {
    const int length = x2 - x1;
    const int head_length = std::max(3, length * 2 / 5);
    const int head_half = std::max(2, length / 3);
    const int shaft_half = std::max(1, length / 14);

    const Brush brush(color);
    canvas.SelectNullPen();
    canvas.Select(brush);

    /* the shaft, up to the base of the head */
    if (right)
      canvas.DrawRectangle(PixelRect{x1, y - shaft_half,
                                     x2 - head_length, y + shaft_half + 1});
    else
      canvas.DrawRectangle(PixelRect{x1 + head_length, y - shaft_half,
                                     x2, y + shaft_half + 1});

    const int tip = right ? x2 : x1;
    const int base = right ? x2 - head_length : x1 + head_length;
    const BulkPixelPoint head[3] = {
      {tip, y},
      {base, y - head_half},
      {base, y + head_half},
    };
    canvas.DrawPolygon(head, 3);
  }
};

/**
 * The sources of an NMEA log (one per device that was connected when
 * it was written), in the order of their rank.  As with the devices
 * in flight, the source at the top has the say for every value it
 * provides and the ones below only fill in what is missing; the user
 * may switch sources off and move them, to see what another
 * arrangement of the devices would have made of the same flight.
 * Changes take effect at once, also in a running replay.
 */
class ReplaySourcesWidget final : public ListWidget {
  NmeaReplaySources &sources;

  Button *toggle_button = nullptr, *up_button = nullptr,
    *down_button = nullptr;

  TwoTextRowsRenderer row_renderer;

public:
  explicit ReplaySourcesWidget(NmeaReplaySources &_sources) noexcept
    :sources(_sources) {}

  void CreateButtons(WidgetDialog &dialog) noexcept {
    toggle_button = dialog.AddButton(_("On/Off"), [this](){
      Toggle(GetList().GetCursorIndex());
    });
    up_button = dialog.AddButton(_("Up"), [this](){
      MoveUp(GetList().GetCursorIndex());
    });
    down_button = dialog.AddButton(_("Down"), [this](){
      MoveDown(GetList().GetCursorIndex());
    });
    dialog.AddButton(_("Close"), mrOK);
  }

private:
  void RefreshView() noexcept {
    /* the replay may have met a new source meanwhile */
    GetList().SetLength(sources.size());
    GetList().Invalidate();
    UpdateButtons();
  }

  void UpdateButtons() noexcept {
    if (toggle_button == nullptr)
      return;

    const unsigned i = GetList().GetCursorIndex();
    toggle_button->SetEnabled(i < sources.size());
    up_button->SetEnabled(i > 0 && i < sources.size());
    down_button->SetEnabled(i + 1 < sources.size());
  }

  void Toggle(unsigned i) noexcept {
    if (i < sources.size())
      sources.Toggle(i);
    RefreshView();
  }

  void MoveUp(unsigned i) noexcept {
    if (i == 0 || i >= sources.size())
      return;
    sources.MoveUp(i);
    GetList().SetCursorIndex(i - 1);
    RefreshView();
  }

  void MoveDown(unsigned i) noexcept {
    if (i + 1 >= sources.size())
      return;
    sources.MoveUp(i + 1);
    GetList().SetCursorIndex(i + 1);
    RefreshView();
  }

public:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override {
    const DialogLook &look = UIGlobals::GetDialogLook();
    CreateList(parent, look, rc,
               row_renderer.CalculateLayout(*look.list.font_bold,
                                            look.small_font));
    RefreshView();
  }

  /* virtual methods from class List::Handler */
  void OnPaintItem(Canvas &canvas, const PixelRect rc,
                   unsigned i) noexcept override {
    if (i >= sources.size())
      return;

    const auto &source = sources[i];

    StaticString<64> title;
    title.Format("%u. %s", i + 1,
                 source.label.empty() ? _("Lines without head")
                 : source.label.c_str());
    row_renderer.DrawFirstRow(canvas, rc, title.c_str());
    row_renderer.DrawRightFirstRow(canvas, rc,
                                   source.enabled ? _("On") : _("Off"));

    /* the first source that is switched on has the say, wherever
       it stands */
    unsigned first_enabled = 0;
    while (first_enabled < sources.size() &&
           !sources[first_enabled].enabled)
      ++first_enabled;

    const char *text;
    if (!source.enabled)
      text = _("Left out of the replay and the NMEA output.");
    else if (i == first_enabled)
      text = _("Has the say for every value it provides.");
    else
      text = _("Fills in what the sources above do not provide.");
    row_renderer.DrawSecondRow(canvas, rc, text);
  }

  void OnCursorMoved([[maybe_unused]] unsigned index) noexcept override {
    UpdateButtons();
  }

  bool CanActivateItem([[maybe_unused]] unsigned index) const noexcept override {
    return true;
  }

  void OnActivateItem(unsigned index) noexcept override {
    Toggle(index);
  }
};

static void
ShowReplaySourcesDialog(NmeaReplaySources &sources) noexcept
{
  TWidgetDialog<ReplaySourcesWidget>
    dialog(WidgetDialog::Full{}, UIGlobals::GetMainWindow(),
           UIGlobals::GetDialogLook(), _("Data sources"));
  dialog.SetWidget(sources);
  dialog.GetWidget().CreateButtons(dialog);
  dialog.EnableCursorSelection();
  dialog.ShowModal();
}

/**
 * The settings of the replay that most pilots never need: what goes
 * out on the "NMEA output" ports and, for an NMEA log, its data
 * sources.  They have a dialog of their own, so that the replay dialog
 * shows only what a plain replay needs.
 */
class ReplayOptionsWidget final : public RowFormWidget {
  enum Controls {
    OUTPUT_CURRENT_TIME,
  };

  Replay &replay;

public:
  ReplayOptionsWidget(Replay &_replay, const DialogLook &look) noexcept
    :RowFormWidget(look), replay(_replay) {}

  /* virtual methods from class Widget */
  void Prepare([[maybe_unused]] ContainerWindow &parent,
               [[maybe_unused]] const PixelRect &rc) noexcept override {
    AddBoolean(_("NMEA output: current time"),
               _("The replayed flight also goes out on the ports with the "
                 "\"NMEA output\" driver, as received data does in flight: "
                 "an NMEA log line by line, an IGC file or a sensor log as "
                 "position, altitude and vario sentences built from it.  "
                 "Data received from the devices is not passed on "
                 "meanwhile.  Off: with the times recorded in the file.  "
                 "On: with the current time and date, so that a device "
                 "behind such a port takes the replay for live data."),
               replay.GetOutputCurrentTime());
    GetDataField(OUTPUT_CURRENT_TIME).SetOnModified([this]{
      const bool value = GetValueBoolean(OUTPUT_CURRENT_TIME);
      replay.SetOutputCurrentTime(value);
      Profile::Set(ProfileKeys::ReplayOutputCurrentTime, value);
      Profile::Save();
    });

    /* only an NMEA log has sources to choose from; the dialog is
       built anew each time, so the row is simply left out otherwise */
    if (!replay.GetNmeaSources().empty())
      AddButton(_("Data sources"), [this](){
        ShowReplaySourcesDialog(replay.GetNmeaSources());
      });
  }
};

static void
ShowReplayOptionsDialog(Replay &replay) noexcept
{
  const DialogLook &look = UIGlobals::GetDialogLook();
  TWidgetDialog<ReplayOptionsWidget>
    dialog(WidgetDialog::Auto{}, UIGlobals::GetMainWindow(), look,
           _("Replay options"));
  dialog.SetWidget(replay, look);
  dialog.AddButton(_("Close"), mrOK);
  dialog.ShowModal();
}

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

  /** shown in place of the position row while seeking or
      fast-forwarding */
  ProgressBar progress_bar;

  UI::PeriodicTimer status_timer{[this]{ UpdateStatus(); }};

  /** the current status update period; short while a seek runs, so
      the progress bar moves smoothly */
  std::chrono::steady_clock::duration status_interval{};

public:
  ReplayControlWidget(Replay &_replay, const DialogLook &look) noexcept
    :RowFormWidget(look), replay(_replay) {}

  void SetButtonPanel(ButtonPanelWidget &_buttons) noexcept {
    buttons_widget = &_buttons;
  }

  /* virtual methods from class Widget */
  void Show(const PixelRect &rc) noexcept override {
    RowFormWidget::Show(rc);
    status_interval = {};
    UpdateStatus();
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

    /* while a seek runs, the progress bar takes the position row's
       place */
    const bool busy = replay.IsActive() && replay.IsSeeking();

    /* update quickly while the bar shows, so it moves smoothly */
    const auto interval = busy
      ? std::chrono::milliseconds(100)
      : std::chrono::milliseconds(500);
    if (interval != status_interval) {
      status_interval = interval;
      status_timer.Schedule(interval);
    }

    if (busy && progress_bar.IsDefined()) {
      const double progress = replay.GetProgress();
      progress_bar.SetValue(progress >= 0
                            ? unsigned(std::clamp(progress, 0., 1.) * 1000)
                            : 0);

      if (!progress_bar.IsVisible()) {
        progress_bar.Move(GetControl(STATUS).GetPosition());
        SetRowVisible(STATUS, false);
        progress_bar.Show();
      }
    } else if (progress_bar.IsDefined() && progress_bar.IsVisible()) {
      progress_bar.Hide();
      SetRowVisible(STATUS, true);
    }

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

  /**
   * The row below the tape deck: back to the previous and on to the
   * next thermal, back to the previous and on to the next glide.  The
   * jumps themselves follow in the next step; for now the buttons
   * show only where they will be.
   */
  void CreateJumpButtons(ButtonPanel &panel) noexcept {
    const ButtonLook &look = GetLook().button;
    using Kind = JumpButtonRenderer::Kind;
    panel.Add(std::make_unique<JumpButtonRenderer>(look, Kind::THERMAL,
                                                   false), []{});
    panel.Add(std::make_unique<JumpButtonRenderer>(look, Kind::THERMAL,
                                                   true), []{});
    panel.Add(std::make_unique<JumpButtonRenderer>(look, Kind::GLIDE,
                                                   false), []{});
    panel.Add(std::make_unique<JumpButtonRenderer>(look, Kind::GLIDE,
                                                   true), []{});
  }

private:
  bool StartFromFile() noexcept;
  void OnPlayPauseClicked() noexcept;
  void OnResetClicked() noexcept;
  void OnTakeoffClicked() noexcept;
  void OnRewindClicked() noexcept;
  void OnFastForwardClicked() noexcept;
  void OnEndClicked() noexcept;

  static bool EditReplayFile(const char *caption, DataField &df,
                             const char *help_text);

public:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override;
};

void
ReplayControlWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                             [[maybe_unused]] const PixelRect &rc) noexcept
{
  WndProperty &file = *AddFile(_("Flight"),
          _("Name of file to replay. May be an IGC file (.igc), a raw NMEA log file (.nmea) or a flight sensor log (.lrsx). Leave blank to run the demo."),
          {},
          {FileType::NMEA, FileType::IGC, FileType::SENSORLOG},
          true);
  file.SetEditCallback(EditReplayFile);
  LoadValue(FILE, replay.GetFilename());
  GetFileDataField(FILE).Sort(FileDataField::SortOrder::DESCENDING, true);

  /* the sources of an NMEA log can be arranged before the replay
     starts; a file chosen while a replay runs takes effect only when
     that replay has ended, and so do its sources */
  if (!replay.IsActive())
    replay.PrepareNmeaSources(GetValueFile(FILE));
  GetDataField(FILE).SetOnModified([this]{
    if (!replay.IsActive()) {
      replay.PrepareNmeaSources(GetValueFile(FILE));
      UpdateStatus();
    }
  });

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

  {
    /* a real progress bar, hidden for now: while a seek or the fast
       forward is under way it appears in place of the position row */
    ContainerWindow &panel = (ContainerWindow &)GetWindow();

    WindowStyle style;
    style.Hide();

    progress_bar.Create(panel,
                        InitialControlRect(Layout::GetMinimumControlHeight()),
                        style);
    progress_bar.SetRange(0, 1000);
  }

  CreateTapeButtons(buttons_widget->GetButtonPanel());
  CreateJumpButtons(buttons_widget->GetButtonPanel());
}

inline bool
ReplayControlWidget::StartFromFile() noexcept
{
  const Path path = GetValueFile(FILE);

  try {
    replay.Start(path, CommonInterface::GetSystemSettings().devices[0]);
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
      replay.Start(path, CommonInterface::GetSystemSettings().devices[0]);
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
      replay.Start(path, CommonInterface::GetSystemSettings().devices[0]);
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
  /* a jump, like "<<" - not the engine's fast forward, which plays
     the span through the computers at high speed */
  if (!replay.IsActive())
    return;

  const TimeStamp t = replay.GetVirtualTime();
  if (!t.IsDefined())
    return;

  replay.SeekTo(t + FloatDuration{std::chrono::minutes{10}});
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

  /* pressed with no replay running, it plays the whole file into
     the trail and the statistics */
  if (!replay.IsActive() && !StartFromFile())
    return;

  auto *merge_thread = backend_components->merge_thread.get();
  auto *calc_thread = backend_components->calculation_thread.get();
  if (merge_thread == nullptr || calc_thread == nullptr)
    return;

  replay.PlayToEnd(*merge_thread, *calc_thread, end_interval);
  UpdateStatus();
}

bool
ReplayControlWidget::EditReplayFile([[maybe_unused]] const char *caption,
                                    DataField &df,
                                    [[maybe_unused]] const char *help_text)
{
  auto &file = static_cast<FileDataField &>(df);
  AllocatedPath path(file.GetValue());
  switch (PickReplayFlight(_("Flight"), path)) {
  case ReplayFlightChoice::CANCEL:
    return false;

  case ReplayFlightChoice::DEMO:
    file.SetIndex(0);
    return true;

  case ReplayFlightChoice::FILE:
    file.ForceModify(path);
    return true;
  }

  return false;
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

  /* the tape deck above, the jumps to thermals and glides below */
  panel->SetBottomRowSplit(6);

  WidgetDialog dialog(WidgetDialog::Auto{}, UIGlobals::GetMainWindow(),
                      look, _("Replay"), panel);

  /* the settings most pilots never need, out of the way */
  dialog.AddButton(_("Options"), [&replay](){
    ShowReplayOptionsDialog(replay);
  });

  /* "Hide" only closes the window - the replay keeps running */
  dialog.AddButton(_("Hide"), mrOK);

  /* "Cancel" ends the replay altogether */
  dialog.AddButton(_("Cancel"), [&replay, &dialog](){
    replay.Stop();
    dialog.SetModalResult(mrCancel);
  });

  dialog.ShowModal();
}
