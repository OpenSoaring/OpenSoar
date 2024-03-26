// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "WindowWidget.hpp"

#include <tchar.h>
#ifdef _UNICODE
#include <string>
#endif

struct DialogLook;

/**
 * A #Widget implementation that displays multi-line text.
 */
class LargeTextWidget : public WindowWidget {
  const DialogLook &look;
  const TCHAR *text;

public:
  explicit LargeTextWidget(const DialogLook &_look,
                           const TCHAR *_text=nullptr) noexcept
    :look(_look), text(_text) {}

  void SetText(const TCHAR *text) noexcept;
#ifdef _UNICODE
  void SetText(const char *text) noexcept;
  void SetText(const std::string_view text) noexcept;
#endif
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool SetFocus() noexcept override;
};
