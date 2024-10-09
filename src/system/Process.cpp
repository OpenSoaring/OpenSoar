// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Process.hpp"

#ifdef HAVE_POSIX

#include <cassert>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#include <stdio.h>
#include <stdlib.h>

#include <exception>
#endif

#define PROCESS_DEBUG_OUTPUT 0
#ifdef __MSVC__
/* August2111: Please use the logging with VS build environment only,
  * all other has to include the FMT component
  * TODO(August2111): remove this debug output asap 
*/
# include "LogFile.hpp"
#endif
#include "system/FileUtil.hpp"

#include <stdarg.h>

#include <sstream>
#include <filesystem>

static Path output;

#include <fcntl.h>
#include <iostream>

#ifdef HAVE_POSIX
static bool
UnblockAllSignals() noexcept
{
  sigset_t ss;
  sigemptyset(&ss);
  return sigprocmask(SIG_SETMASK, &ss, nullptr) == 0;
}

static pid_t
ForkExec(const char *const*argv) noexcept
{
#ifdef __MSVC__
  LogFormat("ForkExec: Call '%s'", argv[0]);
#endif
#ifdef __MSVC__
#endif
  const pid_t pid = fork();
  if (pid == 0) {
    UnblockAllSignals();
    if (!output.empty()) {
      auto fd = open(output.ToUTF8().c_str(), O_WRONLY | O_CREAT,
                     0666);  // open the file
      dup2(fd, STDOUT_FILENO);  // replace standard output with output file
    }
    // exec or die:
    execv(argv[0], const_cast<char **>(argv));
    // ...die:
#ifdef __MSVC__
    LogFormat("ForkExec: After execv => FAIL?");
#endif
    _exit(EXIT_FAILURE);
  }
#ifdef __MSVC__
  LogFormat("ForkExec: pid = %d > 0", pid);
#endif
  return pid;
}

static int
Wait(pid_t pid) noexcept
{
#ifdef __MSVC__
  LogFormat("Process.cpp - Wait: pid = %d (for assert)", pid);
#endif
  assert(pid > 0);

  int status;
  pid_t pid2 = waitpid(pid, &status, 0);
#ifdef __MSVC__
  LogFormat("Process.cpp - Wait: pid2 = %d ", pid2);
#endif
  if (pid2 <= 0)
    return -1;

  if (WIFSIGNALED(status) || !WIFEXITED(status))
    return -1;

  if (!output.empty()) {
    auto fd = dup(STDOUT_FILENO);
    close(fd);
  }
  int ret_value = WEXITSTATUS(status);
#ifdef __MSVC__
  LogFormat("Process.cpp - Wait: return %d", ret_value);
#endif
  return ret_value;
}
#endif

bool
Start(const char *const *argv) noexcept
{
#ifdef HAVE_POSIX
  /* double fork to detach from this process */
  const pid_t pid = fork();
  if (pid < 0) [[unlikely]]
    return false;

  if (pid == 0)
    _exit(ForkExec(argv) ? 0 : 1);

  return Wait(pid) == 0;
#elif defined(_WIN32)
#ifdef __MSVC__
  LogFormat("Process.cpp - on Windows no Start() function");
#endif
  for (unsigned count = 0; argv[count] != nullptr; count++) {
#ifdef __MSVC__
    LogFormat("Process.cpp - Start, Arg %u: %s", count, argv[count]);
#endif
    std::cout << argv[count] << ' ';
  }

  return false;
#else
  error "Unknown system"
#endif
}

int
Run(const char *const *argv) noexcept
try {
#if PROCESS_DEBUG_OUTPUT // def DEBUG_OPENVARIO
  std::cout << "Start Run with:" << std::endl;
  if (!output.empty())
    LogFormat("Process.cpp - Run with output: %s", output.c_str());
  else 
    LogFormat("Process.cpp - Run w/o output");
#endif
  std::stringstream ss;

  for (unsigned count = 0; argv[count] != nullptr; count++) {
    ss << argv[count] << ' ';
    std::cout << argv[count] << ' ';
  }
#if PROCESS_DEBUG_OUTPUT // def DEBUG_OPENVARIO
  std::cout << '!' << std::endl;
  LogFormat("Process.cpp: %s", ss.str().c_str());
#endif

  int ret_value = -1;
#ifdef HAVE_POSIX
  const pid_t pid = ForkExec(argv);
  if (pid > 0)
     ret_value = Wait(pid);
#elif defined(_WIN32)
  ret_value = system(ss.str().c_str());
#else
  error "Unknown system"
#endif
  if (!output.empty())
     output = Path(); // for the next call..
  return ret_value;
} catch (std::exception &e) {
#ifdef __MSVC__
  LogFormat("Process.cpp - exception: %s", e.what());
#endif
  return -1;
}


int 
Run(const Path &output_path, const char *const *argv) noexcept
{
  output = output_path;
  return Run(argv);
}
