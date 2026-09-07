// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "VarioDisplayWidget.hpp"
#include "VarioDisplayWindow.hpp"
#include "Interface.hpp"
#include "UIState.hpp"

void
VarioDisplayWidget::Update() noexcept
{
  VarioDisplayWindow &w = (VarioDisplayWindow &)GetWindow();
  w.ReadBlackboard(CommonInterface::Basic(),
                   CommonInterface::Calculated(),
                   CommonInterface::GetComputerSettings(),
                   CommonInterface::GetMapSettings(),
                   CommonInterface::GetUISettings(),
                   CommonInterface::GetUIState().display_mode);
}

void
VarioDisplayWidget::Prepare(ContainerWindow &parent,
                          const PixelRect &rc) noexcept
{
  WindowStyle style;
  style.Hide();

  auto w = std::make_unique<VarioDisplayWindow>();
  w->Create(parent, rc, style);
  SetWindow(std::move(w));
}

void
VarioDisplayWidget::Show(const PixelRect &rc) noexcept
{
  Update();
  CommonInterface::GetLiveBlackboard().AddListener(*this);

  WindowWidget::Show(rc);
}

void
VarioDisplayWidget::Hide() noexcept
{
  WindowWidget::Hide();

  CommonInterface::GetLiveBlackboard().RemoveListener(*this);
}

void
VarioDisplayWidget::OnGPSUpdate([[maybe_unused]] const MoreData &basic) noexcept
{
  Update();
}

void
VarioDisplayWidget::OnCalculatedUpdate([[maybe_unused]] const MoreData &basic,
                                     [[maybe_unused]] const DerivedInfo &calculated) noexcept
{
  Update();
}

void
VarioDisplayWidget::OnUISettingsUpdate([[maybe_unused]] const UISettings &settings) noexcept
{
  Update();
}
