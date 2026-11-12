// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "StartupDialog.hpp"
#include "Error.hpp"
#include "Form/Button.hpp"
#include "Form/DataField/File.hpp"
#include "Form/Form.hpp"
#include "Gauge/LogoView.hpp"
#include "Language/Language.hpp"
#include "LocalPath.hpp"
#include "CommandLine.hpp"
#include "LogFile.hpp"
#include "Look/DialogLook.hpp"
#include "Profile/Profile.hpp"
#include "Profile/File.hpp"
#include "Profile/Keys.hpp"
#include "Profile/Map.hpp"
#include "Repository/FileType.hpp"
#include "ProfileListDialog.hpp"
#include "ProfilePasswordDialog.hpp"
#include "Screen/Layout.hpp"
#include "UIGlobals.hpp"
#include "Widget/WindowWidget.hpp"
#include "WidgetDialog.hpp"
#include "system/FileUtil.hpp"
#include "SystemConfig.hpp"
#include "UISettings.hpp"
#include "ui/canvas/Canvas.hpp"
#include "ui/control/ProgressBar.hpp"
#include "ui/window/ContainerWindow.hpp"
#include "ui/event/PeriodicTimer.hpp"
#include "util/StaticString.hxx"

/**
 * The start screen: the logo on the background of the progress window
 * that follows it, the version at the top left and "Quit" at the top
 * right, and at the bottom the profile, "Continue" and a bar that counts
 * down to continuing on its own.  The countdown stops at the first
 * touch or key; then the screen waits for a decision.
 */
class StartupWindow final : public ContainerWindow {
  const DialogLook &look;
  WndForm &dialog;
  FileDataField &df;

  LogoView logo;
  PixelRect logo_rect;
  PixelPoint label_position;

  Button quit_button, profile_button, continue_button;
  ProgressBar countdown_bar;

  /**
   * Tenths of a second left until the screen continues with the
   * preselected profile; zero means there is no countdown.
   */
  unsigned countdown;
  const unsigned countdown_total;

  UI::PeriodicTimer countdown_timer{[this]{ OnCountdown(); }};

public:
  StartupWindow(const DialogLook &_look, WndForm &_dialog,
                FileDataField &_df, unsigned timeout_seconds) noexcept
    :look(_look), dialog(_dialog), df(_df),
     countdown(timeout_seconds * 10), countdown_total(timeout_seconds * 10) {}

  void CancelCountdown() noexcept {
    if (countdown == 0)
      return;

    countdown = 0;
    countdown_timer.Cancel();
    countdown_bar.Hide();
  }

  void FocusContinue() noexcept {
    continue_button.SetFocus();
  }

private:
  void UpdateProfileCaption() noexcept {
    profile_button.SetCaption(
      ProfileDisplayName(Path(df.GetAsDisplayString())).c_str());
  }

  void UpdateCountdownBar() noexcept {
    countdown_bar.SetValue(countdown);

    StaticString<64> caption;
    caption.Format("%s (%u)", _("Continue"), (countdown + 9) / 10);
    countdown_bar.SetText(caption);
  }

  void OnCountdown() noexcept {
    if (countdown <= 1) {
      /* CancelCountdown() first: if the profile cannot be used (a
         password-protected one), the screen stays without a stale bar */
      CancelCountdown();
      Continue();
      return;
    }

    --countdown;
    UpdateCountdownBar();
  }

  void ChooseProfile() noexcept;
  void Continue() noexcept;
  void LayoutControls() noexcept;

protected:
  /* virtual methods from class Window */
  void OnCreate() override;
  void OnResize(PixelSize new_size) noexcept override;
  void OnPaint(Canvas &canvas) noexcept override;
};

static bool
SelectProfile(Path path) noexcept
{
  try {
    if (!CheckProfilePasswordResult(CheckProfileFilePassword(path)))
      return false;
  } catch (...) {
    ShowError(std::current_exception(), _("Password"));
    return false;
  }

  Profile::SetFiles(path);

  if (RelativePath(path) == nullptr) {
    /* When a profile from a secondary data path is used, this path
       becomes the primary data path.  Since the data layout migration
       the profiles live in a "profiles" subdirectory, and that
       subdirectory is not the data directory: promoting it would send
       logs, cache and every relative path one level too deep. */
    auto dir = path.GetParent();
    if (dir != nullptr && dir.GetBase() == Path{"profiles"})
      dir = dir.GetParent();

    if (dir != nullptr) {
      LogFmt("Startup dialog: primary data path is now {}", dir.c_str());
      SetPrimaryDataPath(dir);
    }
  }

  File::Touch(path);

  /* remember the choice for the next start screen; the timestamp
     alone is not reliable on a FAT card (two second resolution) */
  SystemConfig::Get().last_profile = path.c_str();
  SystemConfig::Save();

  return true;
}

void
StartupWindow::ChooseProfile() noexcept
{
  /* touching the profile is a decision: no countdown from here on */
  CancelCountdown();

  const auto path = SelectProfileDialog(df.GetValue());
  if (path == nullptr)
    return;

  df.ForceModify(path);
  UpdateProfileCaption();
}

void
StartupWindow::Continue() noexcept
{
  if (SelectProfile(df.GetValue()))
    dialog.SetModalResult(mrOK);
  else
    CancelCountdown();
}

void
StartupWindow::OnCreate()
{
  ContainerWindow::OnCreate();

  const PixelRect rc = GetClientRect();

  WindowStyle style;
  style.TabStop();

  quit_button.Create(*this, look.button, _("Quit"), rc, style,
                     [this]{ dialog.SetModalResult(mrCancel); });
  profile_button.Create(*this, look.button, "", rc, style,
                        [this]{ ChooseProfile(); });
  continue_button.Create(*this, look.button, _("Continue"), rc, style,
                         [this]{ CancelCountdown(); Continue(); });

  WindowStyle bar_style;
  if (countdown == 0)
    bar_style.Hide();
  countdown_bar.Create(*this, rc, bar_style);
  countdown_bar.SetRange(0, countdown_total);

  UpdateProfileCaption();

  if (countdown > 0) {
    UpdateCountdownBar();
    countdown_timer.Schedule(std::chrono::milliseconds{100});
  }
}

void
StartupWindow::LayoutControls() noexcept
{
  const PixelRect rc = GetClientRect();

  logo_rect = rc;
  logo_rect.bottom -= LogoView::GetSplashControlsHeight();

  PixelRect quit_rc = rc;
  quit_rc.left = quit_rc.right - Layout::Scale(75);
  quit_rc.bottom = quit_rc.top + Layout::GetMaximumControlHeight();
  quit_button.Move(quit_rc);

  /* the label "Profile" in front of the profile button, as in a form */
  PixelRect profile_rc = LogoView::GetSplashRow(rc, 0);
  const unsigned label_width = look.text_font.TextSize(_("Profile")).width +
    2 * Layout::GetTextPadding();
  label_position = {
    profile_rc.left,
    profile_rc.top + (int(profile_rc.GetHeight()) -
                      int(look.text_font.GetHeight())) / 2,
  };
  profile_rc.left += label_width;
  profile_button.Move(profile_rc);

  continue_button.Move(LogoView::GetSplashRow(rc, 1));
  countdown_bar.Move(LogoView::GetSplashRow(rc, 2));
}

void
StartupWindow::OnResize(PixelSize new_size) noexcept
{
  ContainerWindow::OnResize(new_size);
  LayoutControls();
}

void
StartupWindow::OnPaint(Canvas &canvas) noexcept
{
  const bool dark = LogoView::DrawSplashBackground(canvas, GetClientRect())
    || look.dark_mode;

  logo.draw(canvas, logo_rect, dark);

  canvas.Select(look.text_font);
  canvas.SetTextColor(dark ? COLOR_WHITE : COLOR_BLACK);
  canvas.SetBackgroundTransparent();
  canvas.DrawText(label_position, _("Profile"));

  ContainerWindow::OnPaint(canvas);
}

class StartupWidget final : public WindowWidget {
  const DialogLook &look;
  WndForm &dialog;
  FileDataField &df;
  const unsigned timeout;

public:
  StartupWidget(const DialogLook &_look, WndForm &_dialog,
                FileDataField &_df, unsigned _timeout) noexcept
    :look(_look), dialog(_dialog), df(_df), timeout(_timeout) {}

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override {
    WindowStyle style;
    style.Hide();
    style.ControlParent();

    auto w = std::make_unique<StartupWindow>(look, dialog, df, timeout);
    w->Create(parent, rc, style);
    SetWindow(std::move(w));
  }

  bool SetFocus() noexcept override {
    /* "Continue" has the focus, so Enter takes the profile shown */
    ((StartupWindow &)GetWindow()).FocusContinue();
    return true;
  }

  bool KeyPress([[maybe_unused]] unsigned key_code) noexcept override {
    /* any key is a decision; the key itself goes to the focused
       button */
    ((StartupWindow &)GetWindow()).CancelCountdown();
    return false;
  }
};

/**
 * How long the dialog waits before it continues with the preselected
 * profile.  The command line wins; otherwise the value comes straight
 * from the profile file - the profile itself is not loaded yet, it is
 * being selected right now.
 */
static unsigned
GetStartupTimeout(Path profile_path) noexcept
{
  if (CommandLine::startup_timeout >= 0)
    return unsigned(CommandLine::startup_timeout);

  if (SystemConfig::IsXCSoarBehaviour())
    /* upstream waits for the user */
    return 0;

  auto value = DEFAULT_STARTUP_TIMEOUT;

  if (profile_path != nullptr) {
    try {
      ProfileMap map;
      Profile::LoadFile(map, profile_path);
      map.Get(ProfileKeys::StartupTimeout, value);
    } catch (...) {
      /* unreadable profile: keep the default */
    }
  }

  return value.count();
}

bool
dlgStartupShowModal() noexcept
{
  LogString("Startup dialog");

  /* scan all profile files */
  auto *dff = new FileDataField();
  dff->SetFileType(FileType::PROFILE);
  dff->ScanDirectoryTop(GetFileTypePatterns(FileType::PROFILE));

  if (dff->GetNumFiles() == 0 && Profile::GetPath() == nullptr) {
    /* no profile exists yet, and none was given: create the default
       profile without asking - there is nothing to choose from */
    Profile::SetFiles(nullptr);
    delete dff;
    return true;
  }

  if (SystemConfig::IsXCSoarBehaviour() && dff->GetNumFiles() == 1) {
    /* upstream behaviour: skip the dialog if there is only one
       profile to choose from */
    const auto path = dff->GetValue();
    if (ProfileFileHasPassword(path) == TriState::FALSE &&
        SelectProfile(path)) {
      delete dff;
      return true;
    }
  }

  /* otherwise the dialog is shown even when there is only one
     profile: it is the start screen, and the countdown below carries
     on by itself */

  unsigned length = dff->size();

  /* a profile given with "-profile=" is preselected ... */
  const Path configured_profile = Profile::GetPath();
  int configured_index = -1;

  if (configured_profile != nullptr)
    for (unsigned i = 0; i < length; ++i)
      if (Path(dff->GetItem(i).path) == configured_profile) {
        configured_index = (int)i;
        break;
      }

  if (configured_index >= 0)
    dff->SetIndex((unsigned)configured_index);
  else if (configured_profile != nullptr)
    /* a profile outside the data directory: add it to the list */
    dff->ForceModify(configured_profile);
  else {
    /* ... otherwise the profile chosen last (recorded in the system
       configuration - see SystemConfig::Settings::last_profile), and
       when there is no such record, the most recently modified one */
    int last_index = -1;
    const auto &last_profile = SystemConfig::Get().last_profile;

    if (!last_profile.empty()) {
      const Path last_path{last_profile.c_str()};
      for (unsigned i = 0; i < length; ++i)
        if (Path(dff->GetItem(i).path) == last_path) {
          last_index = (int)i;
          break;
        }
    }

    if (last_index >= 0)
      dff->SetIndex((unsigned)last_index);
    else {
      unsigned best_index = 0;
      std::chrono::system_clock::time_point best_timestamp =
        std::chrono::system_clock::time_point::min();

      for (unsigned i = 0; i < length; ++i) {
        const auto path = Path(dff->GetItem(i).path);
        const auto timestamp = File::GetLastModification(path);
        if (timestamp > best_timestamp) {
          best_timestamp = timestamp;
          best_index = i;
        }
      }

      dff->SetIndex(best_index);
    }
  }

  const unsigned timeout = GetStartupTimeout(dff->GetValue());

  /* show the start screen */
  const DialogLook &look = UIGlobals::GetDialogLook();
  TWidgetDialog<StartupWidget> dialog(WidgetDialog::Full{},
                                      UIGlobals::GetMainWindow(),
                                      look, nullptr);
  dialog.SetWidget(look, dialog, *dff, timeout);

  const bool ok = dialog.ShowModal() == mrOK;
  delete dff;
  return ok;
}
