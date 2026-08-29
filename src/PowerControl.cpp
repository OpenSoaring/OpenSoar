// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "PowerControl.hpp"
#include "LogFile.hpp"
#include "ExitValues.hpp"

#ifdef KOBO
#include "Kobo/System.hpp"
#endif

#include <iterator>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/*
 * Who performs a restart: the OpenVario base menu started us and
 * starts us again (exit code), Android relaunches its activity before
 * System.exit() (see Android/Main.cpp), everywhere else the process
 * starts itself again.
 */
#if !defined(ANDROID) && !defined(IS_OPENVARIO)
#define HAVE_SELF_RESTART
#endif

static PowerAction pending_action = PowerAction::NONE;

#ifdef HAVE_SELF_RESTART

#ifdef _WIN32

/**
 * The command line for RESTART, once an option was dropped from it;
 * empty means "the one of this process" (GetCommandLine()).
 */
static std::string restart_command_line;

/**
 * Split a Windows command line into its tokens the way the C runtime
 * does, roughly: whitespace separates, double quotes keep it
 * together.  Each token is returned as written (quotes included).
 */
static std::vector<std::string>
SplitCommandLine(const char *src) noexcept
{
  std::vector<std::string> tokens;
  std::string token;
  bool quoted = false;

  for (; *src != '\0'; ++src) {
    const char ch = *src;

    if (ch == '"')
      quoted = !quoted;
    else if (!quoted && (ch == ' ' || ch == '\t')) {
      if (!token.empty()) {
        tokens.emplace_back(std::move(token));
        token.clear();
      }
      continue;
    }

    token.push_back(ch);
  }

  if (!token.empty())
    tokens.emplace_back(std::move(token));

  return tokens;
}

/**
 * The token without its quotes - what the program sees in argv.
 */
static std::string
UnquoteToken(const std::string &token) noexcept
{
  std::string result;
  result.reserve(token.size());
  for (const char ch : token)
    if (ch != '"')
      result.push_back(ch);
  return result;
}

#else

/**
 * A copy of the command line, kept for RESTART: execv() needs it
 * after the original argv is long out of scope.
 */
static std::vector<std::string> restart_args;

#endif

#endif // HAVE_SELF_RESTART

bool
PowerControl::IsAvailable(PowerAction action) noexcept
{
  switch (action) {
  case PowerAction::NONE:
    return false;

  case PowerAction::QUIT:
    return true;

  case PowerAction::RESTART:
    /* everywhere: the desktop and the Kobo exec() themselves, the
       OpenVario base menu does it via exit code, and on Android the
       activity launches a fresh instance right before System.exit()
       (see XCSoar.java onDestroy()) */
    return true;

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

void
PowerControl::DropRestartOption([[maybe_unused]] const char *prefix) noexcept
{
#ifdef HAVE_SELF_RESTART
#ifdef _WIN32
  const char *src = restart_command_line.empty()
    ? GetCommandLine()
    : restart_command_line.c_str();
  if (src == nullptr)
    return;

  auto tokens = SplitCommandLine(src);

  std::string result;
  bool first = true;
  for (const auto &token : tokens) {
    /* the first token is the program itself */
    if (!first && UnquoteToken(token).starts_with(prefix))
      continue;

    if (!result.empty())
      result.push_back(' ');
    result += token;
    first = false;
  }

  restart_command_line = std::move(result);
#else
  if (restart_args.empty())
    return;

  /* argv[0] stays, whatever it looks like */
  for (auto i = std::next(restart_args.begin()); i != restart_args.end();)
    if (i->starts_with(prefix))
      i = restart_args.erase(i);
    else
      ++i;
#endif
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
  const char *src = restart_command_line.empty()
    ? GetCommandLine()
    : restart_command_line.c_str();
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
