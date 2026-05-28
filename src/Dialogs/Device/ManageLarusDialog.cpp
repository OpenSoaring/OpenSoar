// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ManageLarusDialog.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Widget/RowFormWidget.hpp"
#include "UIGlobals.hpp"
#include "Language/Language.hpp"
#include "NMEA/DeviceInfo.hpp"
#include "util/StaticString.hxx"

class ManageLarusWidget final : public RowFormWidget {
  const DeviceInfo info;

public:
  ManageLarusWidget(const DialogLook &look, const DeviceInfo &_info) noexcept
    :RowFormWidget(look), info(_info) {}

  /* virtual methods from Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
};

void
ManageLarusWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                           [[maybe_unused]] const PixelRect &rc) noexcept
{
  /* The Larus reports its firmware version with $PLARF on its own;
     until that sentence has arrived the version is simply not known
     yet, which is not an error. */
  StaticString<64> buffer;
  if (!info.software_version.empty())
    buffer.SetASCII(info.software_version.c_str());
  else
    buffer = _("Unknown");

  AddReadOnly(_("Firmware version"), nullptr, buffer.c_str());
}

void
ManageLarusDialog(const DeviceInfo &info) noexcept
{
  const DialogLook &look = UIGlobals::GetDialogLook();
  WidgetDialog dialog(WidgetDialog::Auto{}, UIGlobals::GetMainWindow(),
                      look, "Larus",
                      new ManageLarusWidget(look, info));
  dialog.AddButton(_("Close"), mrCancel);
  dialog.ShowModal();
}
