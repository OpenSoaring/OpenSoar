// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project
#pragma once
#include "UIGlobals.hpp"
#include "ui/event/Globals.hpp"
#include "ui/display/Display.hpp"
#include "Widget/RowFormWidget.hpp"
#include "Hardware/RotateDisplay.hpp"

class FileMenuWidget final
        : public RowFormWidget
{
    UI::Display &display;
    UI::EventQueue &event_queue;

public:
    FileMenuWidget(UI::Display &_display, UI::EventQueue &_event_queue,
                   const DialogLook &look) noexcept
            :RowFormWidget(look),
    display(_display), event_queue(_event_queue) {}

private:
    /* virtual methods from class Widget */
    void Prepare(ContainerWindow &parent,
                 const PixelRect &rc) noexcept override;

};
