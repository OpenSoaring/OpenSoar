// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Widget/RowFormWidget.hpp"
#include "ui/event/Queue.hpp"
#include "ui/window/SingleWindow.hpp"

class SettingSensordWidget final
  : public RowFormWidget
{
  UI::Display &display;
  UI::EventQueue &event_queue;

public:
  SettingSensordWidget(UI::Display &_display, UI::EventQueue &_event_queue,
                 const DialogLook &look) noexcept
    :RowFormWidget(look),
     display(_display), event_queue(_event_queue) {}

private:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override;
};

