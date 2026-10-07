// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "SystemdConfigPanel.hpp"
#include "OpenVarioSystemWidget.hpp"
#include "OpenVario/System.hpp"
#include "Widget/TwoWidgets.hpp"
#include "Dialogs/Error.hpp"
#include "Dialogs/JobDialog.hpp"
#include "Form/Button.hpp"
#include "Form/ButtonPanel.hpp"
#include "Job/Job.hpp"
#include "Language/Language.hpp"
#include "Linux/SystemdServiceList.hpp"
#include "Look/DialogLook.hpp"
#include "Renderer/TwoTextRowsRenderer.hpp"
#include "UIGlobals.hpp"
#include "Widget/ButtonPanelWidget.hpp"
#include "Widget/ListWidget.hpp"
#include "lib/dbus/AppendIter.hxx"
#include "lib/dbus/CallMethodSync.hxx"
#include "lib/dbus/Connection.hxx"
#include "lib/dbus/Error.hxx"
#include "lib/dbus/Message.hxx"
#include "lib/dbus/Properties.hxx"
#include "lib/dbus/ReadIter.hxx"
#include "lib/dbus/ScopeMatch.hxx"
#include "lib/dbus/Systemd.hxx"
#include "ui/canvas/Canvas.hpp"
#include "ui/canvas/Color.hpp"
#include "ui/event/KeyCode.hpp"
#include "ui/event/PeriodicTimer.hpp"
#include "util/ScopeExit.hxx"
#include "util/StaticString.hxx"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

static constexpr auto refresh_interval = std::chrono::seconds{1};
static constexpr int systemd_action_timeout_ms = 30000;

enum class SystemdAction {
  START,
  STOP,
  RESTART,
};

/**
 * A service of type "simple" counts as started as soon as its program
 * runs, so the start job reports "done" even if the program gives up
 * a moment later (for instance because a device or another daemon it
 * needs is missing).  The unit is therefore watched for this long
 * before the start is taken as successful.
 */
static constexpr auto start_settle_time = std::chrono::milliseconds{1500};
static constexpr auto start_poll_interval = std::chrono::milliseconds{250};

static constexpr const char *systemd_bus_name = "org.freedesktop.systemd1";
static constexpr const char *unit_interface = "org.freedesktop.systemd1.Unit";
static constexpr const char *service_interface =
  "org.freedesktop.systemd1.Service";

/**
 * The object path of a unit, loading it if systemd has dropped it.
 * systemd unloads an inactive unit nobody refers to, so after a failed
 * start the unit may already be gone when its properties are asked
 * for.
 */
static std::string
LoadUnit(ODBus::Connection &connection, const std::string &unit)
{
  auto msg = ODBus::Message::NewMethodCall(systemd_bus_name,
                                           "/org/freedesktop/systemd1",
                                           "org.freedesktop.systemd1.Manager",
                                           "LoadUnit");
  ODBus::AppendMessageIter{*msg.Get()}.Append(unit.c_str());

  auto reply = ODBus::CallMethodSync(connection, msg);
  ODBus::Error error;
  const char *path;
  if (!reply.GetArgs(error, DBUS_TYPE_OBJECT_PATH, &path))
    error.Throw("LoadUnit reply failed");
  return path;
}

/**
 * Keep the unit loaded while this connection is open, so that the
 * result of its main process is still there after it has stopped.
 * Without this, a service that ends right after the start is unloaded
 * and comes back with its properties reset.  Failure is not an error:
 * the explanation then just has less detail.
 */
static void
RefUnit(ODBus::Connection &connection, const std::string &path) noexcept
{
  try {
    auto msg = ODBus::Message::NewMethodCall(systemd_bus_name, path.c_str(),
                                             unit_interface, "Ref");
    (void)ODBus::CallMethodSync(connection, msg);
  } catch (...) {
  }
}

static ODBus::Message
GetUnitProperty(ODBus::Connection &connection, const std::string &path,
                const char *interface, const char *name)
{
  return ODBus::PropertiesGet(connection, systemd_bus_name, path.c_str(),
                              interface, name);
}

static std::string
GetStringProperty(ODBus::Connection &connection, const std::string &path,
                  const char *interface, const char *name)
{
  auto reply = GetUnitProperty(connection, path, interface, name);
  auto iter = ODBus::ReadMessageIter{*reply.Get()}.Recurse();
  if (iter.GetArgType() != DBUS_TYPE_STRING)
    return {};
  return iter.GetString();
}

static int
GetInt32Property(ODBus::Connection &connection, const std::string &path,
                 const char *interface, const char *name)
{
  auto reply = GetUnitProperty(connection, path, interface, name);
  auto iter = ODBus::ReadMessageIter{*reply.Get()}.Recurse();
  if (iter.GetArgType() != DBUS_TYPE_INT32)
    return 0;
  dbus_int32_t value;
  iter.GetBasic(&value);
  return value;
}

static std::vector<std::string>
GetStringArrayProperty(ODBus::Connection &connection, const std::string &path,
                       const char *interface, const char *name)
{
  std::vector<std::string> result;
  auto reply = GetUnitProperty(connection, path, interface, name);
  auto iter = ODBus::ReadMessageIter{*reply.Get()}.Recurse();
  if (iter.GetArgType() != DBUS_TYPE_ARRAY)
    return result;
  iter.Recurse().ForEach(DBUS_TYPE_STRING, [&result](auto &i){
    result.emplace_back(i.GetString());
  });
  return result;
}

static bool
IsServiceUnit(std::string_view unit) noexcept
{
  static constexpr std::string_view suffix{".service"};
  return unit.size() > suffix.size() &&
    unit.substr(unit.size() - suffix.size()) == suffix;
}

/**
 * Why a service whose program has ended is no longer running, as far
 * as systemd knows it: the result of its main process.  Empty for
 * other unit types or if systemd has nothing to say.
 */
static std::string
DescribeServiceEnd(ODBus::Connection &connection, const std::string &unit)
{
  if (!IsServiceUnit(unit))
    return {};

  const auto path = LoadUnit(connection, unit);

  const auto result = GetStringProperty(connection, path, service_interface,
                                        "Result");
  const int status = GetInt32Property(connection, path, service_interface,
                                      "ExecMainStatus");

  StaticString<256> text;
  if (result == "exit-code" && status == 203)
    /* systemd's EXIT_EXEC: the program file could not be run */
    text = _("The program could not be executed.");
  else if (result == "exit-code")
    text.Format(_("The program ended with exit code %d."), status);
  else if (result == "signal" || result == "core-dump")
    text.Format(_("The program was ended by signal %d."), status);
  else if (result == "success")
    text = _("The program ended without reporting an error.");
  else if (result == "start-limit-hit")
    text = _("It was started too often within a short time.");
  else if (result == "resources")
    text = _("systemd could not prepare its start, for instance because a file or directory is missing.");
  else if (!result.empty())
    text.Format(_("systemd reports \"%s\"."), result.c_str());
  else
    return {};

  return text.c_str();
}

/**
 * The units this one requires that are not running, in the order the
 * unit names them.
 */
static std::vector<std::string>
FindMissingRequirements(ODBus::Connection &connection,
                        const std::string &unit)
{
  std::vector<std::string> missing;

  const auto path = LoadUnit(connection, unit);

  for (const char *property : {"Requires", "Requisite", "BindsTo"}) {
    for (auto &name : GetStringArrayProperty(connection, path,
                                             unit_interface, property)) {
      switch (Systemd::GetUnitActiveState(connection, name.c_str())) {
      case Systemd::ActiveState::ACTIVE:
      case Systemd::ActiveState::ACTIVATING:
      case Systemd::ActiveState::RELOADING:
        break;

      case Systemd::ActiveState::INACTIVE:
      case Systemd::ActiveState::FAILED:
      case Systemd::ActiveState::DEACTIVATING:
        missing.emplace_back(std::move(name));
        break;
      }
    }
  }

  return missing;
}

/**
 * The last journal lines of a unit since the given time.  The
 * journal holds what the program itself printed, which usually says
 * best why it gave up.  Empty if there is none or journalctl is not
 * available.  The unit name comes from the allowlist in
 * SystemdServiceList.cpp, so it needs no quoting beyond the quotes.
 */
static std::string
ReadJournal(const std::string &unit, std::time_t since) noexcept
{
  char command[256];
  snprintf(command, sizeof(command),
           "journalctl --no-pager -o cat -n 6 -u '%s' --since @%lld 2>/dev/null",
           unit.c_str(), (long long)since);

  FILE *file = popen(command, "r");
  if (file == nullptr)
    return {};

  std::string text;
  char line[256];
  while (fgets(line, sizeof(line), file) != nullptr)
    text += line;
  pclose(file);

  while (!text.empty() && (text.back() == '\n' || text.back() == ' '))
    text.pop_back();
  return text;
}

/**
 * Throw an exception that says why the unit is not running after a
 * start or restart job ended with the given result.  Returns if the
 * unit runs.
 */
static void
CheckStarted(ODBus::Connection &connection, const std::string &unit,
             const char *display_name, std::string_view job_result,
             std::time_t since)
{
  std::string reason;
  std::string journal_unit = unit;

  if (job_result == "done") {
    auto state = Systemd::ActiveState::ACTIVE;
    for (auto waited = std::chrono::milliseconds::zero();
         waited < start_settle_time; waited += start_poll_interval) {
      std::this_thread::sleep_for(start_poll_interval);
      state = Systemd::GetUnitActiveState(connection, unit.c_str());
      if (state == Systemd::ActiveState::INACTIVE ||
          state == Systemd::ActiveState::FAILED)
        break;
    }

    if (state != Systemd::ActiveState::INACTIVE &&
        state != Systemd::ActiveState::FAILED)
      return;

    reason = _("It stopped again right after the start.");
    if (const auto end = DescribeServiceEnd(connection, unit); !end.empty()) {
      reason += ' ';
      reason += end;
    }
  } else if (job_result == "dependency") {
    const auto missing = FindMissingRequirements(connection, unit);
    if (missing.empty()) {
      reason = _("A service it needs could not be started.");
    } else {
      std::string names;
      for (const auto &name : missing) {
        if (!names.empty())
          names += ", ";
        names += name;
      }

      StaticString<256> text;
      text.Format(_("A service it needs could not be started: %s"),
                  names.c_str());
      reason = text.c_str();

      /* the first missing service is the one whose messages explain
         the failure */
      journal_unit = missing.front();
    }
  } else if (job_result == "failed") {
    reason = DescribeServiceEnd(connection, unit);
    if (reason.empty())
      reason = _("systemd reports the start as failed.");
  } else if (job_result == "timeout") {
    reason = _("systemd gave up waiting for the start.");
  } else if (job_result == "canceled") {
    reason = _("The start was canceled.");
  } else {
    StaticString<128> text;
    text.Format(_("systemd reports \"%s\"."), std::string{job_result}.c_str());
    reason = text.c_str();
  }

  StaticString<256> headline;
  headline.Format(_("%s (%s) could not be started."), display_name,
                  unit.c_str());

  std::string message = headline.c_str();
  message += '\n';
  message += reason;

  if (const auto journal = ReadJournal(journal_unit, since);
      !journal.empty()) {
    StaticString<128> title;
    title.Format(_("Last messages of %s:"), journal_unit.c_str());
    message += "\n\n";
    message += title.c_str();
    message += '\n';
    message += journal;
  }

  throw std::runtime_error{message};
}

class SystemdActionJob final : public Job {
  const SystemdAction action;
  const std::string unit_name;
  const char *const display_name;

public:
  SystemdActionJob(SystemdAction _action, const std::string &_unit_name,
                   const char *_display_name)
    :action(_action), unit_name(_unit_name), display_name(_display_name) {}

  void Run([[maybe_unused]] OperationEnvironment &env) override {
    auto connection = ODBus::Connection::GetSystemPrivate();
    AtScopeExit(&connection) { connection.Close(); };

    const ODBus::ScopeMatch match{connection, Systemd::job_removed_match};

    /* journal entries of earlier runs are not part of the answer;
       one second back covers the rounding to whole seconds */
    const std::time_t since = std::time(nullptr) - 1;

    switch (action) {
    case SystemdAction::START: {
      RefUnit(connection, LoadUnit(connection, unit_name));
      const auto result =
        Systemd::StartUnit(connection, unit_name.c_str(), "replace",
                           systemd_action_timeout_ms);
      CheckStarted(connection, unit_name, display_name, result, since);

      /* only a service that runs is started again at the next boot;
         one that cannot run here would only fail there as well */
      Systemd::EnableUnitFile(connection, unit_name.c_str());
      break;
    }

    case SystemdAction::STOP:
      Systemd::StopUnit(connection, unit_name.c_str(), "replace",
                        systemd_action_timeout_ms);
      Systemd::DisableUnitFile(connection, unit_name.c_str());
      break;

    case SystemdAction::RESTART: {
      RefUnit(connection, LoadUnit(connection, unit_name));
      const auto result =
        Systemd::RestartUnit(connection, unit_name.c_str(), "replace",
                             systemd_action_timeout_ms);
      CheckStarted(connection, unit_name, display_name, result, since);
      break;
    }
    }
  }
};

struct ServiceStatus {
  Systemd::ActiveState state = Systemd::ActiveState::INACTIVE;
  bool valid = false;
};

class SystemdListWidget final : public ListWidget {
  std::vector<SystemdService> services = BuildSystemdServiceList();
  std::vector<ServiceStatus> statuses{services.size()};
  TwoTextRowsRenderer row_renderer;
  Button *toggle_button = nullptr;
  Button *restart_button = nullptr;
  ButtonPanelWidget *button_panel = nullptr;
  bool actions_armed = false;
  UI::PeriodicTimer refresh_timer{[this]{ Refresh(); }};

public:
  void SetButtonPanel(ButtonPanelWidget &_button_panel) noexcept {
    button_panel = &_button_panel;
  }

  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override {
    const auto &look = UIGlobals::GetDialogLook();
    CreateList(parent, look, rc,
               row_renderer.CalculateLayout(look.text_font, look.small_font));
    GetList().SetLength(services.empty() ? 1u : services.size());

    if (button_panel != nullptr)
      CreateButtons(button_panel->GetButtonPanel());
  }

  void Show(const PixelRect &rc) noexcept override {
    ListWidget::Show(rc);
    Refresh();
    refresh_timer.Schedule(refresh_interval);
  }

  void Hide() noexcept override {
    refresh_timer.Cancel();
    DisarmActions();
    ListWidget::Hide();
  }

  void Unprepare() noexcept override {
    refresh_timer.Cancel();
    DisarmActions();
    ListWidget::Unprepare();
  }

  bool KeyPress(unsigned key_code) noexcept override {
    if (key_code == KEY_UP && !actions_armed && !services.empty() &&
        button_panel != nullptr && GetList().HasFocus()) {
      button_panel->GetButtonPanel().EnableCursorSelection();
      actions_armed = true;
      UpdateButtons();
      return true;
    }

    return ListWidget::KeyPress(key_code);
  }

  void OnPaintItem(Canvas &canvas, const PixelRect rc,
                   unsigned idx) noexcept override {
    if (idx >= services.size()) {
      if (services.empty() && idx == 0) {
        row_renderer.DrawFirstRow(canvas, rc,
                                  _("No supported services found"));
        row_renderer.DrawSecondRow(
          canvas, rc, _("No selected systemd units are installed."));
      }

      return;
    }

    const auto &service = services[idx];
    const auto &status = statuses[idx];

    const char *state_text = _("Unavailable");
    Color state_color = COLOR_RED;
    if (status.valid) {
      switch (status.state) {
      case Systemd::ActiveState::ACTIVE:
        state_text = _("On");
        state_color = COLOR_GREEN;
        break;
      case Systemd::ActiveState::INACTIVE:
        state_text = _("Off");
        state_color = canvas.GetTextColor();
        break;
      case Systemd::ActiveState::ACTIVATING:
        state_text = _("Starting...");
        state_color = COLOR_ORANGE;
        break;
      case Systemd::ActiveState::DEACTIVATING:
        state_text = _("Stopping...");
        state_color = COLOR_ORANGE;
        break;
      case Systemd::ActiveState::RELOADING:
        state_text = _("Reloading...");
        state_color = COLOR_ORANGE;
        break;
      case Systemd::ActiveState::FAILED:
        state_text = _("Failed");
        state_color = COLOR_RED;
        break;
      }
    }

    PixelRect name_rc = rc;
    const auto old_color = canvas.GetTextColor();
    canvas.SetTextColor(state_color);
    name_rc.right = row_renderer.DrawRightFirstRow(canvas, rc, state_text);
    canvas.SetTextColor(old_color);
    row_renderer.DrawFirstRow(canvas, name_rc, service.display_name);
    row_renderer.DrawSecondRow(canvas, rc, service.description);
    row_renderer.DrawRightSecondRow(canvas, rc, service.unit_name.c_str());
  }

  void OnCursorMoved([[maybe_unused]] unsigned index) noexcept override {
    UpdateButtons();
  }

  bool CanActivateItem(unsigned index) const noexcept override {
    return CanToggle(index);
  }

  void OnActivateItem(unsigned index) noexcept override {
    Toggle(index);
  }

private:
  void CreateButtons(ButtonPanel &buttons) noexcept {
    toggle_button = buttons.Add(_("Turn on"), [this]{
      Toggle(GetList().GetCursorIndex());
    });
    restart_button = buttons.Add(_("Restart"), [this]{
      Restart(GetList().GetCursorIndex());
    });

    UpdateButtons();
  }

  [[gnu::pure]] bool CanToggle(unsigned index) const noexcept {
    if (index >= statuses.size() || !statuses[index].valid)
      return false;

    switch (statuses[index].state) {
    case Systemd::ActiveState::ACTIVE:
    case Systemd::ActiveState::INACTIVE:
    case Systemd::ActiveState::FAILED:
      return true;

    case Systemd::ActiveState::ACTIVATING:
    case Systemd::ActiveState::DEACTIVATING:
    case Systemd::ActiveState::RELOADING:
      return false;
    }

    return false;
  }

  void UpdateButtons() noexcept {
    const auto index = GetList().GetCursorIndex();
    const bool can_toggle = CanToggle(index);
    const bool is_active = index < statuses.size() && statuses[index].valid &&
      statuses[index].state == Systemd::ActiveState::ACTIVE;

    if (toggle_button != nullptr) {
      toggle_button->SetCaption(is_active ? _("Turn off") : _("Turn on"));
      toggle_button->SetEnabled(can_toggle);
    }

    if (restart_button != nullptr)
      restart_button->SetEnabled(is_active);

    if (actions_armed && button_panel != nullptr)
      button_panel->GetButtonPanel().ReselectToFirstEnabled();
  }

  void DisarmActions() noexcept {
    if (!actions_armed || button_panel == nullptr)
      return;

    button_panel->GetButtonPanel().DisableCursorSelection();
    actions_armed = false;
  }

  void Refresh() noexcept {
    try {
      auto connection = ODBus::Connection::GetSystem();
      for (std::size_t i = 0; i < services.size(); ++i) {
        try {
          statuses[i].state = Systemd::GetUnitActiveState(
            connection, services[i].unit_name.c_str());
          statuses[i].valid = true;
        } catch (...) {
          statuses[i].valid = false;
        }
      }
    } catch (...) {
      for (auto &status : statuses)
        status.valid = false;
    }

    GetList().Invalidate();
    UpdateButtons();
  }

  void Toggle(unsigned index) noexcept {
    if (!CanToggle(index))
      return;

    refresh_timer.Cancel();
    try {
      const auto &unit = services[index].unit_name;
      const auto action = statuses[index].state == Systemd::ActiveState::ACTIVE
        ? SystemdAction::STOP
        : SystemdAction::START;
      SystemdActionJob job{action, unit, services[index].display_name};

      if (!JobDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
                     _("System service"), job))
        throw std::runtime_error{"Failed to start system service job"};

      Refresh();
    } catch (...) {
      ShowError(std::current_exception(), _("System service"));
      Refresh();
    }
    refresh_timer.Schedule(refresh_interval);
  }

  void Restart(unsigned index) noexcept {
    if (index >= statuses.size() || !statuses[index].valid ||
        statuses[index].state != Systemd::ActiveState::ACTIVE)
      return;

    refresh_timer.Cancel();
    try {
      SystemdActionJob job{SystemdAction::RESTART,
                           services[index].unit_name,
                           services[index].display_name};
      if (!JobDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
                     _("System service"), job))
        throw std::runtime_error{"Failed to start system service job"};

      Refresh();
    } catch (...) {
      ShowError(std::current_exception(), _("System service"));
      Refresh();
    }
    refresh_timer.Schedule(refresh_interval);
  }
};

} // namespace

std::unique_ptr<Widget>
CreateSystemdConfigPanel()
{
  auto list = std::make_unique<SystemdListWidget>();
  auto panel = std::make_unique<ButtonPanelWidget>(
    std::move(list), ButtonPanelWidget::Alignment::BOTTOM);
  static_cast<SystemdListWidget &>(panel->GetWidget()).SetButtonPanel(*panel);

  /* on an OpenVario, the services sensord and variod belong together
     with the system functions of the image; they share this page
     instead of getting one of their own */
  if (IsOpenVario())
    return std::make_unique<TwoWidgets>(std::move(panel),
                                        CreateOpenVarioSystemWidget());

  return panel;
}
