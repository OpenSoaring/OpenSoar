// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "OpenVarioSystemWidget.hpp"
#include "Dialogs/Error.hpp"
#include "Form/DataField/Enum.hpp"
#include "Language/Language.hpp"
#include "OV/Calibrate.hpp"
#include "OV/System.hpp"
#include "UIGlobals.hpp"
#include "Widget/RowFormWidget.hpp"

#include <string>

enum ControlIndex {
  IMAGE,
  MAIN_APP,
  CALIBRATE_SENSORS,
};

/**
 * The values of "main_app" which ovmenu-ng.sh knows.  The enum ids
 * index this table.
 */
static constexpr const char *main_app_values[] = {
  "xcsoar",
  "OpenSoar",
};

static constexpr StaticEnumChoice main_app_list[] = {
  { 0, "XCSoar" },
  { 1, "OpenSoar" },
  nullptr
};

class OpenVarioSystemWidget final : public RowFormWidget {
  unsigned main_app;

public:
  OpenVarioSystemWidget() noexcept
    :RowFormWidget(UIGlobals::GetDialogLook()) {}

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;
};

/**
 * Which entry of #main_app_values does the image start?  A missing
 * or unknown value counts as XCSoar: this program is running, so the
 * image has started it.
 */
static unsigned
LoadMainApp() noexcept
try {
  const std::string value = OpenvarioGetMainApp();
  for (unsigned i = 0; i < std::size(main_app_values); ++i)
    if (value == main_app_values[i])
      return i;

  return 0;
} catch (...) {
  return 0;
}

void
OpenVarioSystemWidget::Prepare(ContainerWindow &parent,
                               const PixelRect &rc) noexcept
{
  RowFormWidget::Prepare(parent, rc);

  const std::string image = OpenvarioGetImageName();
  AddReadOnly(_("Firmware image"),
              _("The OpenVario image this device is running."),
              image.empty() ? _("Unknown") : image.c_str());

  main_app = LoadMainApp();
  AddEnum(_("Start after boot"),
          _("The program the OpenVario starts when it is switched on.  "
            "The change takes effect with the next start of the device."),
          main_app_list, main_app);

  AddButton(_("Calibrate sensors"), CalibrateSensors);
}

bool
OpenVarioSystemWidget::Save(bool &_changed) noexcept
{
  if (SaveValueEnum(MAIN_APP, main_app)) {
    try {
      OpenvarioSetMainApp(main_app_values[main_app]);
    } catch (...) {
      ShowError(std::current_exception(), _("Start after boot"));
      return false;
    }

    _changed = true;
  }

  return true;
}

std::unique_ptr<Widget>
CreateOpenVarioSystemWidget() noexcept
{
  return std::make_unique<OpenVarioSystemWidget>();
}
