// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#ifdef IS_OPENVARIO

#include "Form/DataField/Listener.hpp"
#include "Widget/RowFormWidget.hpp"


#include <memory>

class Widget;

#if 1
#include "UIGlobals.hpp"
// -------------------------------------------
class OpenVarioConfigPanel final : public RowFormWidget, DataFieldListener {
public:
  OpenVarioConfigPanel() noexcept : RowFormWidget(UIGlobals::GetDialogLook()) {}

  void SetEnabled(bool enabled) noexcept;

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;

  int OnShow(const UI::SingleWindow &parent) noexcept;

private:
  /* methods from DataFieldListener */
  void OnModified(DataField &df) noexcept override;
};
// ---------------------------------------------
#endif 
std::unique_ptr<Widget>
CreateOpenVarioConfigPanel() noexcept;

#endif