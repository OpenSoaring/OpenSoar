// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Dialogs/DialogSettings.hpp"
#include "Dialogs/Message.hpp"
#include "Dialogs/ProcessDialog.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "DisplayOrientation.hpp"
#include "FileMenuWidget.h"
#include "Hardware/DisplayDPI.hpp"
#include "Hardware/DisplayGlue.hpp"
#include "Hardware/RotateDisplay.hpp"
#include "Look/DialogLook.hpp"
#include "Profile/File.hpp"
#include "Profile/Map.hpp"
#include "Screen/Layout.hpp"
#include "UIGlobals.hpp"
#include "Widget/RowFormWidget.hpp"
#include "system/FileUtil.hpp"
#include "system/Process.hpp"
#include "ui/event/KeyCode.hpp"
#include "ui/event/Queue.hpp"
#include "ui/event/Timer.hpp"
#include "ui/window/Init.hpp"
#include "ui/window/SingleWindow.hpp"

#include "Language/Language.hpp"


#include <map>
#include <string>

class Path;

enum class SSHStatus {
  ENABLED,
  DISABLED,
  TEMPORARY,
};

enum Buttons {
  LAUNCH_SHELL = 100,
  START_UPGRADE = 111,
};

/**
 * Load a system config file and put its variables into a map
*/
void
LoadConfigFile(std::map<std::string, std::string, std::less<>> &map, Path path);
/**
 * Save a map of config variables to a system config file
*/
void
WriteConfigFile(std::map<std::string, std::string, std::less<>> &map, Path path);

uint_least8_t
OpenvarioGetBrightness() noexcept;

void
OpenvarioSetBrightness(uint_least8_t value) noexcept;

DisplayOrientation
OpenvarioGetRotation();

void
OpenvarioSetRotation(DisplayOrientation orientation);

SSHStatus
OpenvarioGetSSHStatus();

void
OpenvarioEnableSSH(bool temporary);

void
OpenvarioDisableSSH();

void GetConfigInt(const std::string &keyvalue, unsigned &value,
                  const TCHAR *path);

void ChangeConfigInt(const std::string &keyvalue, int value,
                  const TCHAR *path);


class SystemMenuWidget final
  : public RowFormWidget
{
  UI::Display &display;
  UI::EventQueue &event_queue;

  WndForm &dialog;

public:
  SystemMenuWidget(UI::Display &_display, UI::EventQueue &_event_queue,
          WndForm &_dialog) noexcept
    :RowFormWidget(_dialog.GetLook()),
     display(_display), event_queue(_event_queue),
     dialog(_dialog) {}

private:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override;
};


class SystemSettingsWidget final
  : public RowFormWidget
{
  UI::Display &display;
  UI::EventQueue &event_queue;

  WndForm &dialog;

public:
  SystemSettingsWidget(UI::Display &_display, UI::EventQueue &_event_queue,
                 WndForm &_dialog) noexcept 
    :RowFormWidget(_dialog.GetLook()),
     display(_display), event_queue(_event_queue),
     dialog(_dialog) {}

private:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override;
};

