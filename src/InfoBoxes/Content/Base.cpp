// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Base.hpp"
extern const struct InfoBoxPanel infobox_panel[];

InfoBoxContent::~InfoBoxContent() noexcept = default;

bool
InfoBoxContent::HandleKey([[maybe_unused]] const InfoBoxKeyCodes keycode) noexcept
{
  return false;
}

void
InfoBoxContent::OnCustomPaint([[maybe_unused]] Canvas &canvas, [[maybe_unused]] const PixelRect &rc) noexcept
{
}

const InfoBoxPanel *
InfoBoxContent::GetDialogContent() noexcept
{
  return infobox_panel;  // the general empty InfoBoxSet
}
