// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "PowerControl.hpp"
#include "LogFile.hpp"
#include "ExitValues.hpp"

#ifdef KOBO
#include "Kobo/System.hpp"
#endif

#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <vector>

#include <unistd.h>
#endif

/*
 * Who performs a restart: the OpenVario base menu started us and
 * starts us again (exit code), everywhere else the process starts
 * itself again.  Android does not restart at all.
 */
#if !defined(ANDROID) && !defined(IS_OPENVARIO)
#define HAVE_SELF_RESTART
#endif

static PowerAction pending_action = PowerAction::NONE;

#if defined(HAVE_SELF_RESTART) && !defined(_WIN32)

/**
 * A copy of the command line, kept for RESTART: execv() needs it
 * after the original argv is long out of scope.
 */
static std::vector<std::string> restart_args;

#endif

bool
PowerControl::IsAvailable(PowerAction action) noexcept
{
  switch (action) {
  case PowerAction::NONE:
    return false;

  case PowerAction::QUIT:
    return true;

  case PowerAction::RESTART:
    /* Android owns the application lifecycle; the process must not
       start itself again behind the system's back */
#ifdef ANDROID
    return false;
#else
    return true;
#endif

  case PowerAction::REBOOT:
  case PowerAction::SHUTDOWN:
    /* only where the program can reach the machine: the Kobo does it
       itself, the OpenVario base menu does it for us */
#if defined(KOBO) || defined(IS_OPENVARIO)
    return true;
#else
    return false;
#endif
  }

  return false;
}

void
PowerControl::Set(PowerAction action) noexcept
{
  pending_action = action;
}

PowerAction
PowerControl::Get() noexcept
{
  return pending_action;
}

void
PowerControl::SaveCommandLine([[maybe_unused]] int argc,
                              [[maybe_unused]] char **argv) noexcept
{
#if defined(HAVE_SELF_RESTART) && !defined(_WIN32)
  restart_args.clear();
  restart_args.reserve(argc);

  for (int i = 0; i < argc; ++i)
    restart_args.emplace_back(argv[i]);
#endif
}

#ifdef HAVE_SELF_RESTART

/**
 * Start a fresh instance of this program.  On POSIX this replaces the
 * running process and never returns on success.
 */
static bool
RestartProgram() noexcept
{
#ifdef _WIN32
  const char *src = GetCommandLine();
  if (src == nullptr)
    return false;

  /* CreateProcess() may modify the command line buffer it gets: hand
     it a copy, not the one owned by the process */
  std::string command_line{src};

  STARTUPINFO si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};

  if (!CreateProcess(nullptr, command_line.data(), nullptr, nullptr, FALSE,
                     0, nullptr, nullptr, &si, &pi))
    return false;

  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  return true;
#else
  if (restart_args.empty())
    return false;

  std::vector<char *> argv;
  argv.reserve(restart_args.size() + 1);
  for (auto &i : restart_args)
    argv.push_back(i.data());
  argv.push_back(nullptr);

#ifdef __linux__
  /* the working directory may have changed since the start, and
     argv[0] may be relative: prefer the kernel's own answer */
  execv("/proc/self/exe", argv.data());
#endif

  execvp(argv.front(), argv.data());

  /* only reached on failure */
  LogString("Restart failed");
  return false;
#endif
}

#endif // HAVE_SELF_RESTART

int
PowerControl::Perform(int exit_code) noexcept
{
  const auto action = pending_action;
  pending_action = PowerAction::NONE;

  switch (action) {
  case PowerAction::NONE:
  case PowerAction::QUIT:
    break;

  case PowerAction::RESTART:
#ifdef IS_OPENVARIO
    /* the base menu started us and starts us again */
    return EXIT_RESTART;
#elif defined(HAVE_SELF_RESTART)
    LogString("Restart");
    RestartProgram();
#endif
    break;

  case PowerAction::REBOOT:
#ifdef KOBO
    KoboReboot();
#elif defined(IS_OPENVARIO)
    return EXIT_REBOOT;
#endif
    break;

  case PowerAction::SHUTDOWN:
#ifdef KOBO
    KoboPowerOff();
#elif defined(IS_OPENVARIO)
    return EXIT_SHUTDOWN;
#endif
    break;
  }

  return exit_code;
}
