// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Calibrate.hpp"
#include "Dialogs/Error.hpp"
#include "Dialogs/Message.hpp"
#include "Dialogs/ProcessDialog.hpp"
#include "Form/Form.hpp"
#include "Language/Language.hpp"
#include "LogFile.hpp"
#include "UIGlobals.hpp"
#include "lib/dbus/Connection.hxx"
#include "lib/dbus/ScopeMatch.hxx"
#include "lib/dbus/Systemd.hxx"
#include "system/Process.hpp"
#include "ui/display/Display.hpp"
#include "ui/event/Queue.hpp"
#include "util/ScopeExit.hxx"
#include "util/StaticString.hxx"

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <stdexcept>

#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

void
CalibrateSensors() noexcept
try {
  /* make sure sensord is stopped while calibrating sensors */
  auto connection = ODBus::Connection::GetSystem();
  const ODBus::ScopeMatch job_removed_match{connection, Systemd::job_removed_match};

  bool has_sensord = false;

  if (Systemd::IsUnitActive(connection, "sensord.socket")) {
    has_sensord = true;

    try {
      Systemd::StopUnit(connection, "sensord.socket");
    } catch (...) {
      std::throw_with_nested(std::runtime_error{"Failed to stop sensord"});
    }
  }

  AtScopeExit(&connection, has_sensord){
    if (has_sensord)
      Systemd::StartUnit(connection, "sensord.socket");
  };

  /* calibrate the sensors */
  static constexpr const char *calibrate_sensors[] = {
    "/opt/bin/sensorcal", "-c", nullptr
  };

  static constexpr int STATUS_BOARD_NOT_INITIALISED = 2;
  static constexpr int RESULT_BOARD_NOT_INITIALISED = 100;
  int result = RunProcessDialog(UIGlobals::GetMainWindow(),
                                UIGlobals::GetDialogLook(),
                                "Calibrate Sensors", calibrate_sensors,
                                [](int status){
                                  return status == STATUS_BOARD_NOT_INITIALISED
                                    ? RESULT_BOARD_NOT_INITIALISED
                                    : 0;
                                });
  if (result != RESULT_BOARD_NOT_INITIALISED)
    return;

  /* initialise the sensors? */
  if (ShowMessageBox("Sensorboard is virgin. Do you want to initialise it?",
                     "Calibrate Sensors", MB_YESNO) != IDYES)
    return;

  static constexpr const char *init_sensors[] = {
    "/opt/bin/sensorcal", "-i", nullptr
  };

  result = RunProcessDialog(UIGlobals::GetMainWindow(),
                            UIGlobals::GetDialogLook(),
                            "Calibrate Sensors", init_sensors,
                            [](int status){
                              return status == EXIT_SUCCESS
                                ? mrOK
                                : 0;
                            });
  if (result != mrOK)
    return;

  /* calibrate again */
  RunProcessDialog(UIGlobals::GetMainWindow(),
                   UIGlobals::GetDialogLook(),
                   "Calibrate Sensors", calibrate_sensors,
                   [](int status){
                     return status == STATUS_BOARD_NOT_INITIALISED
                       ? RESULT_BOARD_NOT_INITIALISED
                       : 0;
                   });
} catch (...) {
  ShowError(std::current_exception(), "Calibrate Sensors");
}

static constexpr const char *touch_calibration_script =
  "/usr/bin/ov-calibrate-ts.sh";

/** the device the calibration script reads, see ov-calibrate-ts.sh */
static constexpr const char *touch_device = "/dev/input/touchscreen0";

/**
 * The calibration is stopped when the touch screen has not been
 * touched for this long: either no panel is connected, or nobody is
 * calibrating.  Long enough to read the instructions and to move from
 * one cross to the next.
 */
static constexpr auto touch_idle_timeout = std::chrono::seconds{30};

/**
 * The calibration is stopped after this long in any case, in case
 * the touch controller reports touches without a panel.
 */
static constexpr auto touch_total_timeout = std::chrono::minutes{3};

bool
HasTouchScreenDevice() noexcept
{
  return access(touch_device, R_OK) == 0;
}

enum class TouchCalibrationResult {
  DONE,
  NO_DEVICE,
  NO_TOUCH,
  TOO_LONG,
  FAILED,
};

/**
 * Read all pending events from the touch device.
 *
 * @return true if one of them is a touch
 */
static bool
ReadTouchEvents(int fd) noexcept
{
  bool touched = false;
  struct input_event events[16];
  ssize_t nbytes;
  while ((nbytes = read(fd, events, sizeof(events))) > 0)
    for (std::size_t i = 0; i < std::size_t(nbytes) / sizeof(events[0]); ++i)
      if (events[i].type == EV_KEY && events[i].code == BTN_TOUCH &&
          events[i].value != 0)
        touched = true;
  return touched;
}

/**
 * Stop the script together with ts_calibrate, which runs in the same
 * process group.
 */
static void
StopProcessGroup(pid_t pid) noexcept
{
  kill(-pid, SIGTERM);

  for (unsigned i = 0; i < 20; ++i) {
    if (waitpid(pid, nullptr, WNOHANG) == pid)
      return;
    usleep(100000);
  }

  kill(-pid, SIGKILL);
  waitpid(pid, nullptr, 0);
}

static TouchCalibrationResult
RunTouchCalibration() noexcept
{
  using Clock = std::chrono::steady_clock;

  /* the touches are watched on the same device the script reads;
     reading it here does not take the events away from ts_calibrate,
     because neither grabs the device */
  const int fd = open(touch_device, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0)
    return TouchCalibrationResult::NO_DEVICE;
  AtScopeExit(fd) { close(fd); };

  const pid_t pid = fork();
  if (pid < 0)
    return TouchCalibrationResult::FAILED;

  if (pid == 0) {
    /* a process group of its own, so the whole script can be stopped */
    setpgid(0, 0);
    execl(touch_calibration_script, touch_calibration_script, nullptr);
    _exit(EXIT_FAILURE);
  }

  setpgid(pid, pid);

  const auto start = Clock::now();
  auto last_touch = start;

  while (true) {
    int status;
    if (waitpid(pid, &status, WNOHANG) == pid)
      return WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS
        ? TouchCalibrationResult::DONE
        : TouchCalibrationResult::FAILED;

    const auto now = Clock::now();
    if (now - start >= touch_total_timeout) {
      StopProcessGroup(pid);
      return TouchCalibrationResult::TOO_LONG;
    }

    if (now - last_touch >= touch_idle_timeout) {
      StopProcessGroup(pid);
      return TouchCalibrationResult::NO_TOUCH;
    }

    struct pollfd pfd{fd, POLLIN, 0};
    if (poll(&pfd, 1, 250) > 0 && (pfd.revents & POLLIN) &&
        ReadTouchEvents(fd))
      last_touch = Clock::now();
  }
}

void
CalibrateTouch(UI::Display &display, UI::EventQueue &event_queue) noexcept
{
  TouchCalibrationResult result;

  {
    const UI::ScopeDropMaster drop_master{display};
    const UI::ScopeSuspendEventQueue suspend_event_queue{event_queue};
    result = RunTouchCalibration();
  }

  /* the messages need the display and the input back */
  StaticString<256> message;
  switch (result) {
  case TouchCalibrationResult::DONE:
    return;

  case TouchCalibrationResult::NO_DEVICE:
    message.Format(_("No touch screen found (%s)."), touch_device);
    break;

  case TouchCalibrationResult::NO_TOUCH:
    message.Format(_("The touch screen was not touched for %u seconds, so the calibration was stopped. Is a touch panel connected?"),
                   unsigned(touch_idle_timeout.count()));
    break;

  case TouchCalibrationResult::TOO_LONG:
    message.Format(_("The calibration took longer than %u minutes and was stopped."),
                   unsigned(touch_total_timeout.count()));
    break;

  case TouchCalibrationResult::FAILED:
    message = _("The touch screen calibration failed.");
    break;
  }

  LogFormat("touch calibration: %s", message.c_str());
  ShowMessageBox(message.c_str(), _("Calibrate touch screen"),
                 MB_OK | MB_ICONEXCLAMATION);
}
