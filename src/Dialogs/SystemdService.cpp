// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "SystemdService.hpp"
#include "Dialogs/Error.hpp"
#include "Dialogs/JobDialog.hpp"
#include "Job/Job.hpp"
#include "Language/Language.hpp"
#include "UIGlobals.hpp"
#include "lib/dbus/AppendIter.hxx"
#include "lib/dbus/CallMethodSync.hxx"
#include "lib/dbus/Connection.hxx"
#include "lib/dbus/Error.hxx"
#include "lib/dbus/Message.hxx"
#include "lib/dbus/Properties.hxx"
#include "lib/dbus/ReadIter.hxx"
#include "lib/dbus/ScopeMatch.hxx"
#include "lib/dbus/Systemd.hxx"
#include "util/ScopeExit.hxx"
#include "util/StaticString.hxx"
#include "util/StringAPI.hxx"

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

static constexpr int systemd_action_timeout_ms = 30000;

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

class SystemdSwitchJob final : public Job {
  const bool on;
  const std::string unit_name;
  const char *const display_name;

public:
  SystemdSwitchJob(bool _on, const std::string &_unit_name,
                   const char *_display_name) noexcept
    :on(_on), unit_name(_unit_name), display_name(_display_name) {}

  void Run([[maybe_unused]] OperationEnvironment &env) override {
    auto connection = ODBus::Connection::GetSystemPrivate();
    AtScopeExit(&connection) { connection.Close(); };

    const ODBus::ScopeMatch match{connection, Systemd::job_removed_match};

    if (!on) {
      Systemd::StopUnit(connection, unit_name.c_str(), "replace",
                        systemd_action_timeout_ms);
      Systemd::DisableUnitFile(connection, unit_name.c_str());
      return;
    }

    /* journal entries of earlier runs are not part of the answer;
       one second back covers the rounding to whole seconds */
    const std::time_t since = std::time(nullptr) - 1;

    /* a restart starts a stopped service as well, and one that runs
       starts again with its current configuration */
    RefUnit(connection, LoadUnit(connection, unit_name));
    const auto result =
      Systemd::RestartUnit(connection, unit_name.c_str(), "replace",
                           systemd_action_timeout_ms);
    CheckStarted(connection, unit_name, display_name, result, since);

    /* only a service that runs is started again at the next boot;
       one that cannot run here would only fail there as well */
    Systemd::EnableUnitFile(connection, unit_name.c_str());
  }
};

} // anonymous namespace

bool
IsSystemdServiceActive(const SystemdService &service) noexcept
try {
  auto connection = ODBus::Connection::GetSystem();
  return Systemd::GetUnitActiveState(connection, service.unit_name.c_str()) ==
    Systemd::ActiveState::ACTIVE;
} catch (...) {
  return false;
}

bool
SwitchSystemdService(const SystemdService &service, bool on) noexcept
{
  try {
    SystemdSwitchJob job{on, service.unit_name, service.display_name};
    if (!JobDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
                   service.display_name, job))
      throw std::runtime_error{"Failed to start system service job"};
  } catch (...) {
    ShowError(std::current_exception(), service.display_name);
  }

  return IsSystemdServiceActive(service);
}

std::optional<SystemdService>
FindSystemdService(const char *id) noexcept
{
  for (auto &service : BuildSystemdServiceList())
    if (StringIsEqual(service.id, id))
      return std::move(service);

  return std::nullopt;
}
