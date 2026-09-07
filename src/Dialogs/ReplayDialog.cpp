// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ReplayDialog.hpp"
#include "Dialogs/Error.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Widget/RowFormWidget.hpp"
#include "UIGlobals.hpp"
#include "Components.hpp"
#include "Replay/Replay.hpp"
#include "Form/DataField/Base.hpp"
#include "Language/Language.hpp"
#include "Repository/FileType.hpp"
#include "system/Path.hpp"
#include "Form/DataField/File.hpp"
#include "ui/event/PeriodicTimer.hpp"
#include "time/BrokenTime.hpp"
#include "util/StaticString.hxx"

#include <algorithm>
#include <chrono>

class ReplayControlWidget final
  : public RowFormWidget
{
  enum Controls {
    FILE,
    RATE,
    STATUS,
  };

  Replay &replay;

  UI::PeriodicTimer status_timer{[this]{ UpdateStatus(); }};

public:
  ReplayControlWidget(Replay &_replay, const DialogLook &look) noexcept
    :RowFormWidget(look), replay(_replay) {}

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
  }

public:
  void CreateButtons(WidgetDialog &dialog) noexcept {
    dialog.AddButton(_("Go"), [this](){ OnGoClicked(); });
    dialog.AddButton(_("Stop"), [this](){ OnStopClicked(); });
    dialog.AddButton(_("Reset"), [this](){ OnResetClicked(); });
    dialog.AddButton(_("Takeoff"), [this](){ OnTakeoffClicked(); });
    dialog.AddButton("+10'", [this](){ OnFastForwardClicked(); });
  }

private:
  bool StartFromFile() noexcept;
  void OnGoClicked() noexcept;
  void OnStopClicked() noexcept;
  void OnResetClicked() noexcept;
  void OnTakeoffClicked() noexcept;
  void OnFastForwardClicked() noexcept;

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
              _("The current fix number, its UTC time, and how much of the file has been played."),
              "-");
}

inline void
ReplayControlWidget::OnStopClicked() noexcept
{
  if (replay.IsActive() && replay.IsPaused()) {
    /* a second "Stop" while paused ends the replay altogether */
    replay.Stop();
  } else {
    /* pause only - "Go" continues from here, "Reset" rewinds */
    replay.SetTimeScale(0);
  }
  UpdateStatus();
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
ReplayControlWidget::OnGoClicked() noexcept
{
  if (!replay.IsActive()) {
    if (!StartFromFile())
      return;
  }

  /* "Go" also resumes after "Stop" */
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
 * playing when it was playing, otherwise it waits there for "Go".
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
ReplayControlWidget::OnFastForwardClicked() noexcept
{
  replay.FastForward(std::chrono::minutes{10});
}

void
ShowReplayDialog(Replay &replay) noexcept
{
  const DialogLook &look = UIGlobals::GetDialogLook();
  ReplayControlWidget *widget = new ReplayControlWidget(replay, look);
  WidgetDialog dialog(WidgetDialog::Auto{}, UIGlobals::GetMainWindow(),
                      look, _("Replay"), widget);
  widget->CreateButtons(dialog);
  dialog.AddButton(_("Close"), mrOK);

  dialog.ShowModal();
}
