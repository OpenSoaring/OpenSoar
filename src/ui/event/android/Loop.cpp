// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Loop.hpp"
#include "Queue.hpp"
#include "../shared/Event.hpp"
#include "../Timer.hpp"
#include "ui/window/TopWindow.hpp"

namespace UI {

bool
EventLoop::Get(Event &event)
{
  if (queue.IsQuit())
    return false;

  if (bulk) {
    if (queue.Pop(event))
      return true;

    /* that was the last event for now, refresh the screen now */
    top_window.Refresh();
    bulk = false;
  }

  if (queue.Wait(event)) {
    bulk = true;
    return true;
  }

  return false;
}

void
EventLoop::Dispatch(const Event &event)
{
  if (event.type == Event::TIMER) {
    Timer *timer = (Timer *)event.ptr;
    timer->Invoke();
  } else if (event.type == Event::CALLBACK_) {
    event.callback(event.ptr);
  } else if (event.type != Event::NOP)
    top_window.OnEvent(event);
}

} // namespace UI
