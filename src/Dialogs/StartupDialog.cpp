// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

// #define GLIDER_SELECTION

#include "StartupDialog.hpp"
#include "Error.hpp"
#include "ProfilePasswordDialog.hpp"
#include "ProfileListDialog.hpp"
// #include "Dialogs/Plane/PlaneListDialog.hpp"
#include "WidgetDialog.hpp"
#include "Widget/TwoWidgets.hpp"
#include "Widget/RowFormWidget.hpp"
#include "UIGlobals.hpp"
#include "Profile/Profile.hpp"
#include "ui/canvas/Canvas.hpp"
#include "Screen/Layout.hpp"
#include "Look/DialogLook.hpp"
#include "Form/Form.hpp"
#include "Form/Button.hpp"
#include "Form/DataField/File.hpp"
#include "Language/Language.hpp"
#include "Gauge/LogoView.hpp"
#include "LogFile.hpp"
#include "LocalPath.hpp"
#include "system/FileUtil.hpp"

#include "ui/event/PeriodicTimer.hpp"
#include "Widget/ProgressWidget.hpp"
#include "Operation/PluggableOperationEnvironment.hpp"
#include "MainWindow.hpp"

class StartupWidget;
static StartupWidget *startup_widget = nullptr;

class LogoWindow final : public PaintWindow {
  LogoView logo;

protected:
  void OnPaint(Canvas &canvas) noexcept override {
    canvas.ClearWhite();
    logo.draw(canvas, GetClientRect());
  }
};

class LogoQuitWidget final : public NullWidget {
  const ButtonLook &look;
  WndForm &dialog;

  LogoWindow logo;
  Button quit;

public:
  LogoQuitWidget(const ButtonLook &_look, WndForm &_dialog) noexcept
    :look(_look), dialog(_dialog) {}

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
    PROGRESS,
    PROFILE,
#ifdef GLIDER_SELECTION
    GLIDER,
#endif
    CONTINUE,
  };

  WndForm &dialog;
  DataField *const df;

#ifdef GLIDER_SELECTION
  DataField *dfg = nullptr;
#endif
  const uint32_t max_wait_time = 4*1000;  // in ms
  const uint32_t timer_step = 200;  // in ms
  uint32_t wait_time = 0;
  Button *btnContinue = nullptr;
  WndProperty *profile_edit = nullptr;

public:
#ifdef GLIDER_SELECTION
  StartupWidget(const DialogLook &look, WndForm &_dialog,
                DataField *_dfp,DataField *_dfg) noexcept
    :RowFormWidget(look), dialog(_dialog), df(_dfp) , dfg(_dfg) {}
#else
  StartupWidget(const DialogLook &look, WndForm &_dialog,
                DataField *_dfp) noexcept
    :RowFormWidget(look), dialog(_dialog), df(_dfp)
  {
    startup_widget = this;
  }
#endif


  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;

  bool SetFocus() noexcept override {
    /* focus the "Profile" edit by default */
    profile_edit->SetFocus();
    return true;
  }

  void
  Close(int value) {
    dialog.SetFocus();
    dialog.SetModalResult(value);
  }
  void
  RemoveTimer() {
    timer_closing = false; // don't increment the wait_time anymore
    wait_time = 0;
    startup_loader_env->SetText("");
    startup_loader_env->SetProgressPosition(0);
  }

  void OnTimer() {
    if (timer_closing)
      wait_time += timer_step;

    startup_loader_env->SetProgressRange(max_wait_time);
    startup_loader_env->SetProgressPosition(max_wait_time - wait_time);

    if (force_closing ||
      (wait_time >= max_wait_time)) {
#ifdef _DEBUG
      Beep(440, 200);
#endif
      Close(mrOK);
    }
    if (!HasFocus() ||!profile_edit->HasFocus()) {
      if (timer.IsActive() && wait_time > 0)
         RemoveTimer();
    }
  }

  UI::PeriodicTimer timer{ [this] {OnTimer(); } };

  std::unique_ptr<PluggableOperationEnvironment> startup_loader_env;
  std::unique_ptr<Widget> progress;

#if 1
  bool force_closing = false;
  bool timer_closing = true;
};
#else
  static bool force_closing;
  bool timer_closing = true;
};
bool StartupWidget::force_closing = false;

#endif

static bool
SelectProfileCallback([[maybe_unused]] const char *caption, DataField &_df,
                      [[maybe_unused]] const char *help_text) noexcept
{

  FileDataField &df = (FileDataField &)_df;

  if (startup_widget != nullptr) {
    startup_widget->RemoveTimer();
  }
  const auto path = SelectProfileDialog(df.GetValue());
  if (path == nullptr)
    return false;

  df.ForceModify(path);
  if (startup_widget != nullptr) {
      startup_widget->force_closing = true;
  }
  return true;
}

#ifdef GLIDER_SELECTION
static bool
SelectPlaneCallback([[maybe_unused]] const char *caption, [[maybe_unused]] DataField &_dfg,
                      [[maybe_unused]] const char *help_text) noexcept
{
  FileDataField &dfg = (FileDataField &)_dfg;
  
  const auto path = dfg.GetValue(); // Path("(D-KMWH)");
  // const auto path = Path("(D-KMWH)"); // nullptr; // SelectPlaneDialog(df.GetValue());
  //const auto path = SelectPlaneDialog(dfg.GetValue());
  if (path == nullptr)
    return false;
  
  dfg.ForceModify(path);

  return true;
}
#endif


void
StartupWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                       [[maybe_unused]] const PixelRect &rc) noexcept
{
  startup_loader_env = std::make_unique<PluggableOperationEnvironment>();
  progress = std::make_unique<ProgressWidget>(*startup_loader_env, _("Select Profile Setting..."));
  Add(std::move(progress));
 
  profile_edit = Add(_("Profile"), nullptr, df);
  profile_edit->SetEditCallback(SelectProfileCallback);
  
#ifdef GLIDER_SELECTION
  auto *ge = Add(_("Glider"), nullptr, dfg);
  ge->SetEditCallback(SelectPlaneCallback);
#endif

  btnContinue = AddButton(_("Continue"), dialog.MakeModalResultCallback(mrOK));

  timer.Schedule(std::chrono::milliseconds(timer_step));
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

  if (RelativePath(path) == nullptr)
    /* When a profile from a secondary data path is used, this path
       becomes the primary data path */
    SetPrimaryDataPath(path.GetParent());

  /* change the file date of profile file w/ touch, so the next call can
     detect the path as 'the best item' */
#if  1  // def __NO_TEST__
  File::Touch(path);
#endif
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

bool
dlgStartupShowModal(Path profile) noexcept
{
  LogString("Startup dialog");

  /* scan all profile files */
  auto *dff = new FileDataField();
  dff->ScanDirectoryTop("*.prf");

#ifdef GLIDER_SELECTION
  /* scan all glider/plant files */
  auto *dfg = new FileDataField();
  dfg->ScanDirectoryTop("*.xcp");
#endif

  if (dff->GetNumFiles() == 1) {
    /* skip this dialog if there is only one */
    const auto path = dff->GetValue();
    if (ProfileFileHasPassword(path) == TriState::FALSE &&
      SelectProfile(path)) {
      delete dff;
      return true;
    }
  }
  else if (dff->GetNumFiles() == 0) {
    /* no profile exists yet: create default profile */
    Profile::SetFiles(nullptr);
    return true;
  }

  /* preselect the most recently used profile */
  unsigned best_index = 0;
  if (profile != nullptr && File::Exists(profile)) {
    dff->SetValue(profile);
  } else {
    std::chrono::system_clock::time_point best_timestamp =
      std::chrono::system_clock::time_point::min();
    unsigned length = dff->size();

    for (unsigned i = 0; i < length; ++i) {
      const auto path = dff->GetItem(i);
      const auto timestamp = File::GetLastModification(path);
      if (timestamp > best_timestamp) {
        best_timestamp = timestamp;
        best_index = i;
      }
    }
    dff->SetIndex(best_index);
  }
  
#ifdef GLIDER_SELECTION
  // auto num_glider = dfg->GetNumFiles();
  if (dfg->GetNumFiles() > 0)
    dfg->SetIndex(0);
#endif

  /* show the dialog */
  const DialogLook &look = UIGlobals::GetDialogLook();
  TWidgetDialog<TwoWidgets> dialog(WidgetDialog::Full{},
                                   UIGlobals::GetMainWindow(),
                                   UIGlobals::GetDialogLook(),
                                   nullptr);

#ifdef GLIDER_SELECTION
  dialog.SetWidget(std::make_unique<LogoQuitWidget>(look.button, dialog),
                   std::make_unique<StartupWidget>(look, dialog, dff, dfg));
#else
  dialog.SetWidget(std::make_unique<LogoQuitWidget>(look.button, dialog),
                   std::make_unique<StartupWidget>(look, dialog, dff));
#endif

  return dialog.ShowModal() == mrOK;
}
