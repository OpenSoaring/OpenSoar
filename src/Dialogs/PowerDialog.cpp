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
#include "Widget/RowFormWidget.hpp"
#include "util/StaticString.hxx"

/**
 * The buttons of the power dialog.  Which of them exist depends on
 * the target - see PowerControl::IsAvailable().
 */
class PowerWidget final : public RowFormWidget {
  WndForm &dialog;
  PowerAction &action;

public:
  PowerWidget(const DialogLook &look, WndForm &_dialog,
              PowerAction &_action) noexcept
    :RowFormWidget(look), dialog(_dialog), action(_action) {}

private:
  void AddAction(PowerAction _action, const char *label) noexcept {
    if (!PowerControl::IsAvailable(_action))
      return;

    AddButton(label, [this, _action]{
      action = _action;
      dialog.SetModalResult(mrOK);
    });
  }

public:
  /* virtual methods from class Widget */
  void Prepare([[maybe_unused]] ContainerWindow &parent,
               [[maybe_unused]] const PixelRect &rc) noexcept override {
    AddAction(PowerAction::QUIT, _("Quit"));
    AddAction(PowerAction::RESTART, _("Restart"));
    AddAction(PowerAction::REBOOT, _("Reboot"));
    AddAction(PowerAction::SHUTDOWN, _("Shutdown"));
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
  const char *text;

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
AskPowerAction() noexcept
{
  PowerAction action = PowerAction::NONE;

  TWidgetDialog<PowerWidget> dialog(WidgetDialog::Auto{},
                                    UIGlobals::GetMainWindow(),
                                    UIGlobals::GetDialogLook(),
                                    _("Exit"));
  dialog.SetWidget(UIGlobals::GetDialogLook(), dialog, action);
  dialog.AddButton(_("Cancel"), mrCancel);

  if (dialog.ShowModal() != mrOK || action == PowerAction::NONE)
    return false;

  /* the action itself happens after the program has shut down, so
     that the settings are saved first (PowerControl::Perform()) */
  PowerControl::Set(action);
  BeginShutdownFeedback(action);
  return true;
}

void
ShowPowerDialog() noexcept
{
  if (AskPowerAction())
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
