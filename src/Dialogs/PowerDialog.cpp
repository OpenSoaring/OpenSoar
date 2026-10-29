// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "PowerDialog.hpp"
#include "Message.hpp"
#include "WidgetDialog.hpp"
#include "Form/Form.hpp"
#include "Language/Language.hpp"
#include "PowerControl.hpp"
#include "ProgressGlue.hpp"
#include "ProductName.hpp"
#include "UIActions.hpp"
#include "UIGlobals.hpp"
#include "Asset.hpp"
#include "Form/Button.hpp"
#include "Look/DialogLook.hpp"
#include "Screen/Layout.hpp"
#include "ui/control/ProgressBar.hpp"
#include "ui/event/KeyCode.hpp"
#include "ui/event/PeriodicTimer.hpp"
#include "Widget/Widget.hpp"
#include "util/StaticString.hxx"

#include <boost/container/static_vector.hpp>

#include <chrono>

/**
 * The time after which the dialog carries out the preferred action by
 * itself: the pilot who reaches for the power dialog after the flight
 * and walks away still gets the device switched off, and a pilot who
 * opened it by mistake has ten seconds and a Cancel button.
 */
static constexpr unsigned COUNTDOWN_SECONDS = 10;

/**
 * The actions of the power dialog as a grid of buttons, two per row
 * as in XCSoar's exit dialog, with a progress bar below that counts
 * down to the preferred action.  Which actions exist depends on the
 * target - see PowerControl::IsAvailable().
 *
 * The preferred action is the button that has the focus: at first the
 * one the dialog was opened for (a key such as "X" for shutdown) or
 * the first one, later whichever button the pilot moves the focus to,
 * with the cursor keys or a rotary knob.  Moving the focus starts the
 * countdown again for the new button; moving it to Cancel counts down
 * to closing the dialog without an action.  Pressing a button carries
 * out its action at once.
 */
class PowerWidget final : public NullWidget {
  struct Entry {
    PowerAction action;
    const char *label;
  };

  WndForm &dialog;
  PowerAction &action;
  const PowerAction preselect;

  /**
   * The Cancel button of the dialog, which takes part in the focus
   * rule above; nullptr until the dialog has added it.
   */
  Window *cancel_button = nullptr;

  boost::container::static_vector<Entry, 4> entries;
  boost::container::static_vector<Button, 4> buttons;
  ProgressBar progress_bar;

  /** index into #entries, or -1 for Cancel */
  int preferred = 0;

  /** the countdown in ticks, see TICK */
  unsigned remaining = 0;

  UI::PeriodicTimer timer{[this]{ OnTick(); }};

  /**
   * One step of the countdown.  The focus is looked at in every step,
   * so a short step makes the bar follow the focus quickly; e-paper
   * redraws slowly and gets one step per second.
   */
  [[gnu::pure]]
  static std::chrono::milliseconds GetTick() noexcept {
    return HasEPaper()
      ? std::chrono::milliseconds{1000}
      : std::chrono::milliseconds{250};
  }

  [[gnu::pure]]
  static unsigned GetTicksPerSecond() noexcept {
    return 1000 / GetTick().count();
  }

public:
  PowerWidget(WndForm &_dialog, PowerAction &_action,
              PowerAction _preselect) noexcept
    :dialog(_dialog), action(_action), preselect(_preselect) {}

  void SetCancelButton(Window *_cancel_button) noexcept {
    cancel_button = _cancel_button;
  }

private:
  void AddEntry(PowerAction _action, const char *label) noexcept {
    if (!PowerControl::IsAvailable(_action))
      return;

    if (_action == preselect)
      preferred = entries.size();
    entries.push_back({_action, label});
  }

  void Choose(PowerAction _action) noexcept {
    timer.Cancel();
    action = _action;
    dialog.SetModalResult(mrOK);
  }

  [[gnu::pure]]
  unsigned GetRows() const noexcept {
    return (entries.size() + 1) / 2;
  }

  /**
   * The rectangle of each button and of the progress bar.  A row with
   * a single button (an odd number of actions) gives it the full
   * width.
   */
  void UpdateLayout(const PixelRect &rc) noexcept {
    const unsigned n = buttons.size();
    const unsigned rows = GetRows();
    const unsigned gap = Layout::GetTextPadding();
    const unsigned bar_height = Layout::GetMinimumControlHeight();
    const unsigned grid_height = rc.GetHeight() > bar_height + gap
      ? rc.GetHeight() - bar_height - gap
      : rc.GetHeight();
    const unsigned row_height = rows > 0 ? grid_height / rows : 0;

    for (unsigned i = 0; i < n; ++i) {
      const unsigned row = i / 2, column = i % 2;
      const bool alone = column == 0 && i + 1 == n;
      PixelRect b;
      b.top = rc.top + row * row_height;
      b.bottom = b.top + row_height - gap;
      if (alone) {
        b.left = rc.left;
        b.right = rc.right;
      } else {
        const int middle = rc.left + int(rc.GetWidth()) / 2;
        b.left = column == 0 ? rc.left : middle + int(gap) / 2;
        b.right = column == 0 ? middle - int(gap) / 2 : rc.right;
      }
      buttons[i].Move(b);
    }

    progress_bar.Move({rc.left, rc.bottom - int(bar_height),
                       rc.right, rc.bottom});
  }

  void Restart() noexcept {
    remaining = COUNTDOWN_SECONDS * GetTicksPerSecond();
    progress_bar.SetRange(0, remaining);
    UpdateProgress();
  }

  void UpdateProgress() noexcept {
    progress_bar.SetValue(remaining);

    const unsigned seconds =
      (remaining + GetTicksPerSecond() - 1) / GetTicksPerSecond();
    StaticString<64> text;
    text.Format(_("%s in %u s"),
                preferred >= 0 ? entries[preferred].label : _("Cancel"),
                seconds);
    progress_bar.SetText(text);
  }

  /** Which button has the focus: an index, -1 for Cancel, or the
      current preference if the focus is somewhere else */
  [[gnu::pure]]
  int GetFocusedChoice() const noexcept {
    for (unsigned i = 0; i < buttons.size(); ++i)
      if (buttons[i].HasFocus())
        return i;

    if (cancel_button != nullptr && cancel_button->HasFocus())
      return -1;

    return preferred;
  }

  void OnTick() noexcept {
    if (const int focused = GetFocusedChoice(); focused != preferred) {
      preferred = focused;
      Restart();
      return;
    }

    if (remaining > 1) {
      --remaining;
      UpdateProgress();
      return;
    }

    timer.Cancel();
    if (preferred >= 0)
      Choose(entries[preferred].action);
    else
      dialog.SetModalResult(mrCancel);
  }

public:
  /* virtual methods from class Widget */
  PixelSize GetMinimumSize() const noexcept override {
    const unsigned height = Layout::GetMinimumControlHeight();
    return {2 * Layout::Scale(80u),
            (GetRows() + 1) * (height + Layout::GetTextPadding())};
  }

  PixelSize GetMaximumSize() const noexcept override {
    const unsigned height = Layout::GetMaximumControlHeight();
    return {2 * Layout::Scale(160u),
            (GetRows() + 1) * (height + Layout::GetTextPadding())};
  }

  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override {
#ifdef IS_OPENVARIO
    /* the OpenVario is a fixed instrument: the pilot reaches for this
       dialog to switch the device off after the flight, so that comes
       first; quitting to the console is the rare case */
    AddEntry(PowerAction::SHUTDOWN, _("Shutdown"));
    AddEntry(PowerAction::REBOOT, _("Reboot"));
    AddEntry(PowerAction::QUIT, _("Quit"));
    AddEntry(PowerAction::RESTART, _("Restart"));
#else
    AddEntry(PowerAction::QUIT, _("Quit"));
    AddEntry(PowerAction::RESTART, _("Restart"));
    AddEntry(PowerAction::REBOOT, _("Reboot"));
    AddEntry(PowerAction::SHUTDOWN, _("Shutdown"));
#endif

    const auto &look = UIGlobals::GetDialogLook();
    WindowStyle style;
    style.Hide();
    style.TabStop();
    for (const auto &entry : entries) {
      const PowerAction a = entry.action;
      buttons.emplace_back(parent, look.button, entry.label, rc, style,
                           [this, a]{ Choose(a); });
    }

    WindowStyle bar_style;
    bar_style.Hide();
    progress_bar.Create(parent, rc, bar_style);
  }

  void Show(const PixelRect &rc) noexcept override {
    UpdateLayout(rc);
    for (auto &button : buttons)
      button.Show();
    progress_bar.Show();

    Restart();
    timer.Schedule(GetTick());
  }

  void Hide() noexcept override {
    timer.Cancel();
    for (auto &button : buttons)
      button.Hide();
    progress_bar.Hide();
  }

  void Move(const PixelRect &rc) noexcept override {
    UpdateLayout(rc);
  }

  bool SetFocus() noexcept override {
    if (buttons.empty())
      return false;

    buttons[preferred >= 0 ? preferred : 0].SetFocus();
    return true;
  }

  bool HasFocus() const noexcept override {
    for (const auto &button : buttons)
      if (button.HasFocus())
        return true;
    return false;
  }

  /**
   * The cursor keys move through the grid: left and right within a
   * row, up and down between the rows, and down from the last row to
   * Cancel.
   */
  bool KeyPress(unsigned key_code) noexcept override {
    const int focused = GetFocusedChoice();
    if (focused < 0 || unsigned(focused) >= buttons.size() ||
        !buttons[focused].HasFocus())
      return false;

    int next = -1;
    switch (key_code) {
    case KEY_LEFT:
      if (focused % 2 == 1)
        next = focused - 1;
      break;

    case KEY_RIGHT:
      if (focused % 2 == 0 && unsigned(focused + 1) < buttons.size())
        next = focused + 1;
      break;

    case KEY_UP:
      if (focused >= 2)
        next = focused - 2;
      break;

    case KEY_DOWN:
      if (unsigned(focused + 2) < buttons.size())
        next = focused + 2;
      else if (focused % 2 == 1 && unsigned(focused + 1) < buttons.size())
        /* from the right column onto a single button below */
        next = focused + 1;
      break;

    default:
      return false;
    }

    if (next < 0) {
      if (key_code == KEY_DOWN && cancel_button != nullptr) {
        cancel_button->SetFocus();
        return true;
      }

      return key_code == KEY_LEFT || key_code == KEY_RIGHT;
    }

    buttons[next].SetFocus();
    return true;
  }
};

/**
 * Closing takes a few seconds (settings, devices, logger): say so
 * immediately, while the program is still running - once the shutdown
 * has begun, ProgressGlue refuses to create its window.
 */
static void
BeginShutdownFeedback(PowerAction action) noexcept
{
  /* preset, so the compiler does not have to prove that the switch
     below covers every enumerator */
  const char *text = _("Shutdown, please wait...");

  switch (action) {
  case PowerAction::RESTART:
    text = _("Restarting, please wait...");
    break;

  case PowerAction::REBOOT:
    text = _("Rebooting, please wait...");
    break;

  case PowerAction::SHUTDOWN:
    text = _("Switching off, please wait...");
    break;

  case PowerAction::NONE:
  case PowerAction::QUIT:
    text = _("Shutdown, please wait...");
    break;
  }

  ProgressGlue::Create(text);
}

bool
AskPowerAction(PowerAction preselect) noexcept
{
  PowerAction action = PowerAction::NONE;

  TWidgetDialog<PowerWidget> dialog(WidgetDialog::Auto{},
                                    UIGlobals::GetMainWindow(),
                                    UIGlobals::GetDialogLook(),
                                    _("Exit"));
  dialog.SetWidget(dialog, action, preselect);
  dialog.GetWidget().SetCancelButton(dialog.AddButton(_("Cancel"),
                                                      mrCancel));

  if (dialog.ShowModal() != mrOK || action == PowerAction::NONE)
    return false;

  /* the action itself happens after the program has shut down, so
     that the settings are saved first (PowerControl::TakeRestart() and
     PowerControl::Perform()) */
  PowerControl::Set(action);
  BeginShutdownFeedback(action);
  return true;
}

void
ShowPowerDialog(PowerAction preselect) noexcept
{
  if (AskPowerAction(preselect))
    UIActions::SignalShutdown(true);
}

bool
OfferRestart(const char *message) noexcept
{
  StaticString<256> text;

  if (!PowerControl::IsAvailable(PowerAction::RESTART)) {
    text.Format("%s\n%s", message,
                _("The change takes effect on the next start."));
    ShowMessageBox(text, PRODUCT_NAME, MB_OK);
    return false;
  }

  text.Format("%s\n%s", message,
              _("Restart now?"));

  if (ShowMessageBox(text, PRODUCT_NAME,
                     MB_YESNO | MB_ICONQUESTION) != IDYES)
    return false;

  PowerControl::Set(PowerAction::RESTART);
  BeginShutdownFeedback(PowerAction::RESTART);
  UIActions::SignalShutdown(true);
  return true;
}

void
RestartNow(const char *message) noexcept
{
  const PowerAction action = PowerControl::IsAvailable(PowerAction::RESTART)
    ? PowerAction::RESTART
    : PowerAction::QUIT;

  StaticString<256> text;
  text.Format("%s\n%s", message,
              action == PowerAction::RESTART
              ? _("The program restarts now.")
              : _("The program quits now, please start it again."));
  ShowMessageBox(text, PRODUCT_NAME, MB_OK | MB_ICONINFORMATION);

  PowerControl::Set(action);
  BeginShutdownFeedback(action);
  UIActions::SignalShutdown(true);
}
