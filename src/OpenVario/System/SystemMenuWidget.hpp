// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#define OV_SETTINGS 0

#include "Widget/RowFormWidget.hpp"
#include "ui/event/Queue.hpp"
#include "ui/window/SingleWindow.hpp"

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
  // void CalibrateSensors() noexcept;
};
