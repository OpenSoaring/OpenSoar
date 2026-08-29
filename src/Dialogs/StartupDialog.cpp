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
#include "Widget/RowFormWidget.hpp"
#include "Widget/TwoWidgets.hpp"
#include "WidgetDialog.hpp"
#include "system/FileUtil.hpp"
#include "SystemConfig.hpp"
#include "UISettings.hpp"
#include "ui/canvas/Canvas.hpp"
#include "ui/event/PeriodicTimer.hpp"
#include "util/StaticString.hxx"

class LogoWindow final : public PaintWindow {
  LogoView logo;
  bool dark_mode;

public:
  explicit LogoWindow(bool _dark_mode = false,
                     Color _background_color = COLOR_WHITE) noexcept
    :dark_mode(_dark_mode), background_color(_background_color) {}

protected:
  void OnPaint(Canvas &canvas) noexcept override {
    canvas.Clear(background_color);
    logo.draw(canvas, GetClientRect(), dark_mode);
  }

private:
  Color background_color;
};

class LogoQuitWidget final : public NullWidget {
  const ButtonLook &look;
  WndForm &dialog;

  LogoWindow logo;
  Button quit;

public:
  LogoQuitWidget(const ButtonLook &_look, WndForm &_dialog,
                 bool dark_mode, Color background_color) noexcept
    :look(_look), dialog(_dialog), logo(dark_mode, background_color) {}

private:
  PixelRect GetButtonRect(PixelRect rc) noexcept {
    rc.left = rc.right - Layout::Scale(75);
    rc.bottom = rc.top + Layout::GetMaximumControlHeight();
    return rc;
  }

public:
  /* virtual methods from class Widget */
  PixelSize GetMinimumSize() const noexcept override {
    return { 150, 150 };
  }

  PixelSize GetMaximumSize() const noexcept override {
    /* use as much as possible */
    return { 8192, 8192 };
  }

  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override {
    WindowStyle style;
    style.Hide();

    WindowStyle button_style(style);
    button_style.Hide();
    button_style.TabStop();

    quit.Create(parent, look, _("Quit"), rc,
                button_style, dialog.MakeModalResultCallback(mrCancel));
    logo.Create(parent, rc, style);
  }

  void Show(const PixelRect &rc) noexcept override {
    quit.MoveAndShow(GetButtonRect(rc));
    logo.MoveAndShow(rc);
  }

  void Hide() noexcept override {
    quit.FastHide();
    logo.FastHide();
  }

  void Move(const PixelRect &rc) noexcept override {
    quit.Move(GetButtonRect(rc));
    logo.Move(rc);
  }
};

class StartupWidget final : public RowFormWidget {
  enum Controls {
    PROFILE,
    CONTINUE,
  };

  WndForm &dialog;
  DataField *const df;

  /**
   * Seconds left until the dialog continues with the preselected
   * profile; zero means there is no countdown (the user decides).
   */
  unsigned countdown;

  UI::PeriodicTimer countdown_timer{[this]{ OnCountdown(); }};

public:
  StartupWidget(const DialogLook &look, WndForm &_dialog,
                DataField *_df, unsigned _timeout) noexcept
    :RowFormWidget(look), dialog(_dialog), df(_df), countdown(_timeout) {}

  ~StartupWidget() noexcept;

  /**
   * Any user input stops the countdown: from here on the dialog waits
   * for a decision.
   */
  void CancelCountdown() noexcept {
    if (countdown == 0)
      return;

    countdown = 0;
    countdown_timer.Cancel();
    ((Button &)GetRow(CONTINUE)).SetCaption(_("Continue"));
  }

private:
  void UpdateContinueButton() noexcept {
    StaticString<64> caption;
    caption.Format("%s (%u)", _("Continue"), countdown);
    ((Button &)GetRow(CONTINUE)).SetCaption(caption);
  }

  void OnCountdown() noexcept {
    if (countdown <= 1) {
      /* CancelCountdown() first: if Save() fails (a password-protected
         profile), the dialog stays open without a stale counter */
      CancelCountdown();
      dialog.SetModalResult(mrOK);
      return;
    }

    --countdown;
    UpdateContinueButton();
  }

public:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;

  void Show(const PixelRect &rc) noexcept override {
    RowFormWidget::Show(rc);

    if (countdown > 0) {
      UpdateContinueButton();
      countdown_timer.Schedule(std::chrono::seconds{1});
    }
  }

  void Hide() noexcept override {
    countdown_timer.Cancel();
    RowFormWidget::Hide();
  }

  bool KeyPress(unsigned key_code) noexcept override {
    /* the key itself is still handled by the focused control */
    CancelCountdown();
    return RowFormWidget::KeyPress(key_code);
  }

  bool SetFocus() noexcept override {
    /* focus the "Continue" button by default */
    GetRow(CONTINUE).SetFocus();
    return true;
  }
};

/**
 * The startup dialog exists once at a time; the profile editor is a
 * plain callback function and reaches the widget through this.
 */
static StartupWidget *startup_widget;

StartupWidget::~StartupWidget() noexcept
{
  if (startup_widget == this)
    startup_widget = nullptr;
}

static bool
SelectProfileCallback([[maybe_unused]] const char *caption, [[maybe_unused]] DataField &_df,
                      [[maybe_unused]] const char *help_text) noexcept
{
  /* touching the profile is a decision: no countdown from here on */
  if (startup_widget != nullptr)
    startup_widget->CancelCountdown();

  FileDataField &df = (FileDataField &)_df;

  const auto path = SelectProfileDialog(df.GetValue());
  if (path == nullptr)
    return false;

  df.ForceModify(path);
  return true;
}

void
StartupWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                       [[maybe_unused]] const PixelRect &rc) noexcept
{
  startup_widget = this;

  auto *pe = Add(_("Profile"), nullptr, df);
  pe->SetEditCallback(SelectProfileCallback);

  AddButton(_("Continue"), dialog.MakeModalResultCallback(mrOK));
}

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

bool
StartupWidget::Save(bool &changed) noexcept
{
  const auto &dff = (const FileDataField &)GetDataField(PROFILE);
  if (!SelectProfile(dff.GetValue()))
    return false;

  changed = true;

  return true;
}

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

  /* show the dialog */
  const DialogLook &look = UIGlobals::GetDialogLook();
  TWidgetDialog<TwoWidgets> dialog(WidgetDialog::Full{},
                                   UIGlobals::GetMainWindow(),
                                   UIGlobals::GetDialogLook(),
                                   nullptr);

  dialog.SetWidget(std::make_unique<LogoQuitWidget>(look.button, dialog,
                                                    look.dark_mode, look.background_color),
                   std::make_unique<StartupWidget>(look, dialog, dff, timeout));

  return dialog.ShowModal() == mrOK;
}
