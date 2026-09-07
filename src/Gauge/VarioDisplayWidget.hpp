// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Widget/WindowWidget.hpp"
#include "Blackboard/BlackboardListener.hpp"

/**
 * The full screen page showing the big round vario display, fed
 * from XCSoar's own values (VarioDisplayWindow).
 */
class VarioDisplayWidget final : public WindowWidget,
                               private NullBlackboardListener {
  void Update() noexcept;

public:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  void Show(const PixelRect &rc) noexcept override;
  void Hide() noexcept override;

private:
  /* virtual methods from class BlackboardListener */
  void OnGPSUpdate(const MoreData &basic) noexcept override;
  void OnCalculatedUpdate(const MoreData &basic,
                          const DerivedInfo &calculated) noexcept override;
  void OnUISettingsUpdate(const UISettings &settings) noexcept override;
};
