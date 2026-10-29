// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

/**
 * This is the main entry point for the application
 * @file XCSoar.cpp
 */

#include "Startup.hpp"
#include "LocalPath.hpp"
#include "Version.hpp"
#include "LogFile.hpp"
#include "CommandLine.hpp"
#include "PowerControl.hpp"
#include "SystemConfig.hpp"
#include "MainWindow.hpp"
#include "ui/window/ContainerWindow.hpp"
#include "Interface.hpp"
#include "Look/GlobalFonts.hpp"
#include "ui/window/Init.hpp"
#include "ui/event/Queue.hpp"
#include "net/http/Init.hpp"
#include "ResourceLoader.hpp"
#include "Language/Language.hpp"
#include "Language/LanguageGlue.hpp"
#include "Simulator.hpp"
#include "Audio/GlobalPCMMixer.hpp"
#include "Audio/GlobalPCMResourcePlayer.hpp"
#include "Audio/GlobalVolumeController.hpp"
#include "Dialogs/DataManagement/ExportFlightsPanel.hpp"
#include "system/Args.hpp"
#include "io/async/GlobalAsioThread.hpp"
#include "io/async/AsioThread.hpp"
#include "util/PrintException.hxx"

#ifdef ENABLE_SDL
#ifdef SDL_MAIN_HANDLED
/* When SDL_MAIN_HANDLED is defined, we must call SDL_SetMainReady()
   before using SDL to avoid SDL's -Dmain=SDL_main
   macro which conflicts with "main" in the XCSoar code */
#include <SDL.h>
#else
/* this is necessary on macOS, to let libSDL bootstrap Quartz
   before entering our main() */
#include <SDL_main.h>
#endif
#endif

#ifdef __APPLE__
#include <TargetConditionals.h>
#if !TARGET_OS_IPHONE
#include "Apple/MacOSMainMenu.hpp"
#endif
#endif

#include <cassert>

static int
Main()
{
  /* must happen before any other thread is created; see
     UI::BlockSignals() */
  UI::BlockSignals();

  ScreenGlobalInit screen_init;

#ifdef _WIN32
  /* try to make the UI most responsive */
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
#endif

  AllowLanguage();
  InitLanguage();

#if defined(__APPLE__) && !TARGET_OS_IPHONE
  InitialiseMacOSMainMenu();
#endif

  ScopeGlobalAsioThread global_asio_thread;
  const Net::ScopeInit net_init(asio_thread->GetEventLoop());

  ScopeGlobalPCMMixer global_pcm_mixer(asio_thread->GetEventLoop());
  ScopeGlobalPCMResourcePlayer global_pcm_resouce_player;
  ScopeGlobalVolumeController global_volume_controller;

  // Perform application initialization and run loop
  int ret = EXIT_FAILURE;
  if (Startup(screen_init.GetDisplay())) {
    ret = CommonInterface::main_window->RunEventLoop();

    /* Code that hands the next step over to the script which started
       the program (on the OpenVario: firmware upgrade, touch
       calibration, reboot after switching the main app) announces it
       with ContainerWindow::SetExitValue().  The event loop itself
       always returns 0, so the value has to be picked up here, or the
       script never learns what it was asked to do. */
    if (const unsigned exit_value = ContainerWindow::GetExitValue();
        exit_value != 0) {
      LogFormat("Leaving with exit value %u", exit_value);
      ret = exit_value;
    }
  } else if (WasStartupCancelledByUser())
    /* quitting from the startup dialogs is a deliberate user action,
       not an error */
    ret = EXIT_SUCCESS;

  /* The export-flight cache owns an InjectTask on the Asio event loop. */
  ShutdownExportFlightsPanel();

  Shutdown();

  DisallowLanguage();

  Fonts::Deinitialize();

  DeinitialiseDataPath();

  return ret;
}

/**
 * Main entry point for the whole XCSoar application
 */
#ifndef _WIN32
int main(int argc, char **argv)
#else
int WINAPI
WinMain([[maybe_unused]] HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance,
        [[maybe_unused]] LPSTR lpCmdLine2,
        [[maybe_unused]] int nCmdShow)
#endif
try {
#if defined(ENABLE_SDL) && defined(SDL_MAIN_HANDLED)
  SDL_SetMainReady();
#endif

#ifdef USE_WIN32_RESOURCES
  ResourceLoader::Init(hInstance);
#endif

  // Read options from the command line
  {
#ifdef _WIN32
    Args args(GetCommandLine(), CommandLine::OptionSummary());
#else
    Args args(argc, argv, CommandLine::OptionSummary());
#endif
    CommandLine::Parse(args);
  }

#ifdef IS_OPENVARIO
  /* what the command line said, before the system settings below get a
     say - each run of a restart decides anew */
  const auto touch_input_from_command_line = CommandLine::touch_input;
#endif

  /* "Restart" runs Main() once more within this process instead of
     ending it, as OpenSoar did up to 7.44: no new process, no help
     from the OpenVario base menu, and on every target the same */
  int ret;
  while (true) {
    CommandLine::ApplyDataPath();
    InitialiseDataPath();

    /* a "-profile=" on the command line applies to the first run only;
       a restart picks the profile the user chose since */
    CommandLine::ApplyPendingProfile();

    /* device settings (not part of any profile) decide how the program
       starts and how it is left */
    SystemConfig::Load();

#ifdef IS_OPENVARIO
    /* the OpenVario cannot tell whether a touch panel is connected (see
       SystemConfig::Settings::touch_screen); a switch on the command
       line still wins */
    CommandLine::touch_input = touch_input_from_command_line;
    if (!SystemConfig::Get().touch_screen &&
        CommandLine::touch_input == CommandLine::TouchInput::Auto)
      CommandLine::touch_input = CommandLine::TouchInput::Disable;
#endif

    // Write startup note + version to logfile
    LogFormat("Starting %s", XCSoar_ProductToken);

    ret = Main();

    if (!PowerControl::TakeRestart())
      break;

    LogString("Restart");
    PrepareRestart();
  }

  ret = PowerControl::Perform(ret);

#if defined(__APPLE__) && TARGET_OS_IPHONE
  /* For some reason, the app process does not exit on iOS, but a black
   * screen remains, if the process is not explicitly terminated */
  exit(ret);
#endif

  return ret;
} catch (...) {
  PrintException(std::current_exception());
  return EXIT_FAILURE;
}
