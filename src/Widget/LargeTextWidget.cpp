// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "LargeTextWidget.hpp"
#include "ui/control/LargeTextWindow.hpp"
#include "Look/DialogLook.hpp"

#ifdef _UNICODE
# include "util/ConvertString.hpp"
#endif
void
LargeTextWidget::SetText(const TCHAR *text) noexcept
{
  LargeTextWindow &w = (LargeTextWindow &)GetWindow();
  w.SetText(text);
}

#ifdef _UNICODE
// Maybe this is against MaxK XCSoar rules, but this makes the life much easier
void
LargeTextWidget::SetText(const char *text) noexcept
{
  SetText(ConvertACPToWide(text).c_str());
}

void
LargeTextWidget::SetText(std::string_view text) noexcept
{
  SetText(text.data());
}
#endif

void
LargeTextWidget::Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept
{
  LargeTextWindowStyle style;
  style.Hide();
  style.TabStop();

  auto w = std::make_unique<LargeTextWindow>();
  w->Create(parent, rc, style);
  w->SetFont(look.text_font);
  if (text != nullptr)
    w->SetText(text);

  SetWindow(std::move(w));
}

bool
LargeTextWidget::SetFocus() noexcept
{
  GetWindow().SetFocus();
  return true;
}
