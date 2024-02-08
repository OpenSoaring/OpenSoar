// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#define OV_SETTINGS 0

#if OV_SETTINGS 

#include "Widget/RowFormWidget.hpp"
#include "UIGlobals.hpp"
#include "ui/event/Queue.hpp"
#include "ui/display/Display.hpp"
#include "Look/DialogLook.hpp"

// static DialogSettings dialog_settings;
// static UI::SingleWindow *global_main_window;
// static DialogLook *global_dialog_look;

// const DialogSettings &UIGlobals::GetDialogSettings();
// const DialogLook &UIGlobals::GetDialogLook();
// UI::SingleWindow &UIGlobals::GetMainWindow();



class SystemMenuWidget final : public RowFormWidget {
  UI::Display &display;
  UI::EventQueue &event_queue;

public:
  SystemMenuWidget(UI::Display &_display, UI::EventQueue &_event_queue,
                   const DialogLook &look) noexcept
      : RowFormWidget(look), display(_display), event_queue(_event_queue) {}

private:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
};

#endif