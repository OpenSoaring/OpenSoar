// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#ifdef KOBO
#include "linux/MergeMouse.hpp"
#include "linux/Input.hpp"
#else
#include "libinput/LibInputHandler.hpp"
#endif

#include "ui/dim/Point.hpp"

#include <cstdint>

enum class DisplayOrientation : uint8_t;

namespace UI {

class EventQueue;
struct Event;

class InputEventQueue final {
#ifdef KOBO
  MergeMouse merge_mouse;
  LinuxInputDevice keyboard;
  LinuxInputDevice mouse;
#else
  LibInputHandler libinput_handler;
#endif /* !USE_LIBINPUT */

public:
  explicit InputEventQueue(EventQueue &queue) noexcept;
  ~InputEventQueue() noexcept;

  void SetScreenSize(PixelSize screen_size) noexcept {
  #ifdef USE_LIBINPUT
    libinput_handler.SetScreenSize(screen_size);
  #elif defined(_WIN32)
    // TODO(August2111): needs work?
  #else
    merge_mouse.SetScreenSize(screen_size);
  #endif
  }

#ifndef USE_LIBINPUT
  void SetDisplayOrientation(DisplayOrientation orientation) {
#if defined(_WIN32)
    // TODO(August2111): needs work?
#else
    merge_mouse.SetDisplayOrientation(orientation);
#endif
  }
#endif

  bool HasPointer() const noexcept {
#ifdef USE_LIBINPUT
    return libinput_handler.HasPointer();
#elif defined(_WIN32)
    // TODO(August2111): needs work?
    return false;
#else
    return merge_mouse.HasPointer();
#endif
  }

#ifdef USE_LIBINPUT
  bool HasTouchScreen() const noexcept {
    return libinput_handler.HasTouchScreen();
  }

  bool HasKeyboard() const noexcept {
    return libinput_handler.HasKeyboard();
  }
#endif

  PixelPoint GetMousePosition() const noexcept {
#ifdef USE_LIBINPUT
    return PixelPoint(libinput_handler.GetX(), libinput_handler.GetY());
#elif defined(_WIN32)
    // TODO(August2111): needs work?
    return PixelPoint(0, 0);
#else
    return merge_mouse.GetPosition();
#endif
  }

  void Suspend() noexcept {
#ifdef USE_LIBINPUT
    libinput_handler.Suspend();
#endif
  }

  void Resume() noexcept {
#ifdef USE_LIBINPUT
    libinput_handler.Resume();
#endif
  }

  bool Generate(Event &event) noexcept;
};

} // namespace UI
