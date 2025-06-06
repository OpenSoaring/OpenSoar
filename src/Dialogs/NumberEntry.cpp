// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Dialogs/NumberEntry.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Widget/FixedWindowWidget.hpp"
#include "Form/DigitEntry.hpp"
#include "Language/Language.hpp"
#include "Math/Angle.hpp"
#include "UIGlobals.hpp"

enum DataType{
  DATA_SIGNED,
  DATA_UNSIGNED,
  DATA_ANGLE,
};

// ----------------------------------------------------------------------------
template <class T>
bool
NumberEntryDialog(TWidgetDialog<FixedWindowWidget> &dialog,
                  const DataType type,
                  T &value, unsigned length)
{
  ContainerWindow &client_area = dialog.GetClientAreaWindow();

  //create the input control
  WindowStyle control_style;
  control_style.Hide();
  control_style.TabStop();

  auto entry = std::make_unique<DigitEntry>(UIGlobals::GetDialogLook());
  switch (type) {
  case DATA_SIGNED:
    entry->CreateSigned(client_area, client_area.GetClientRect(), control_style,
                        length, 0);
    break;
  case DATA_ANGLE:  // TODO(August2111): check of correctness
  case DATA_UNSIGNED:
      entry->CreateUnsigned(client_area, client_area.GetClientRect(),
                          control_style, length, 0);
      break;
  }
  entry->Resize(entry->GetRecommendedSize());
  entry->SetValue(value);
  entry->SetCallback(dialog.MakeModalResultCallback(mrOK));

  // create buttons
  dialog.first_button = dialog.AddButton(_("OK"), mrOK);
  dialog.last_button = dialog.AddButton(_("Cancel"), mrCancel);

  // set handler for cursor overflow
  entry->SetLeftOverflow(dialog.SetFocusButtonCallback(dialog.last_button));
  entry->SetRightOverflow(dialog.SetFocusButtonCallback(dialog.first_button));

  // run it
  dialog.SetWidget(std::move(entry));

  return (dialog.ShowModal() == mrOK);
}

// ----------------------------------------------------------------------------
/** NumberEntryDialog for big signed numbers  -> SIGNED with +/-! */
bool
NumberEntryDialog(const char *caption, int &value, unsigned length) {
  TWidgetDialog<FixedWindowWidget> dialog(WidgetDialog::Auto{},
                                          UIGlobals::GetMainWindow(),
                                          UIGlobals::GetDialogLook(), caption);
  if (!NumberEntryDialog(dialog, DATA_SIGNED, value, length))
      return false;
  value = ((DigitEntry &)dialog.GetWidget().GetWindow()).GetIntegerValue();
  return true;
}

// ----------------------------------------------------------------------------
/** NumberEntryDialog for big unsigned numbers -> UNSIGNED! */
bool
#if 1  //  TODO(August2111) check the merge 2024-09-03
NumberEntryDialog(const char *caption, unsigned &value, unsigned length) {
  TWidgetDialog<FixedWindowWidget> dialog(WidgetDialog::Auto{},
                                          UIGlobals::GetMainWindow(),
                                          UIGlobals::GetDialogLook(), caption);
  if (!NumberEntryDialog(dialog, DATA_UNSIGNED, value, length))
      return false;
#else
NumberEntryDialog(const char *caption,
                  unsigned &value, unsigned length)
{
  /* create the dialog */

  const DialogLook &look = UIGlobals::GetDialogLook();

  TWidgetDialog<FixedWindowWidget>
    dialog(WidgetDialog::Auto{}, UIGlobals::GetMainWindow(), look, caption);

  ContainerWindow &client_area = dialog.GetClientAreaWindow();

  /* create the input control */

  WindowStyle control_style;
  control_style.Hide();
  control_style.TabStop();

  auto entry = std::make_unique<DigitEntry>(look);
  entry->CreateUnsigned(client_area, client_area.GetClientRect(), control_style,
                        length, 0);
  entry->Resize(entry->GetRecommendedSize());
  entry->SetValue(value);
  entry->SetCallback(dialog.MakeModalResultCallback(mrOK));

  /* create buttons */

  dialog.AddButton(_("OK"), mrOK);
  dialog.AddButton(_("Cancel"), mrCancel);

  /* run it */

  dialog.SetWidget(std::move(entry));

  bool result = dialog.ShowModal() == mrOK;
  if (!result)
    return false;
#endif
  value = ((DigitEntry &)dialog.GetWidget().GetWindow()).GetUnsignedValue();
  return true;
}

// ----------------------------------------------------------------------------
/** NumberEntryDialog for big angle values */
bool
#if 1  //  TODO(August2111) check the merge 2024-09-03
AngleEntryDialog(const char *caption, Angle &value) {
  TWidgetDialog<FixedWindowWidget> dialog(WidgetDialog::Auto{},
                                          UIGlobals::GetMainWindow(),
                                          UIGlobals::GetDialogLook(), caption);
  if (!NumberEntryDialog(dialog, DATA_ANGLE, value, 0))
      return false;
#else
AngleEntryDialog(const char *caption, Angle &value)
{
  /* create the dialog */

  const DialogLook &look = UIGlobals::GetDialogLook();

  TWidgetDialog<FixedWindowWidget>
    dialog(WidgetDialog::Auto{}, UIGlobals::GetMainWindow(), look, caption);

  ContainerWindow &client_area = dialog.GetClientAreaWindow();

  /* create the input control */

  WindowStyle control_style;
  control_style.Hide();
  control_style.TabStop();

  auto entry = std::make_unique<DigitEntry>(look);
  entry->CreateAngle(client_area, client_area.GetClientRect(), control_style);
  entry->Resize(entry->GetRecommendedSize());
  entry->SetValue(value);
  entry->SetCallback(dialog.MakeModalResultCallback(mrOK));

  /* create buttons */

  dialog.AddButton(_("OK"), mrOK);
  dialog.AddButton(_("Cancel"), mrCancel);

  /* run it */

  dialog.SetWidget(std::move(entry));

  bool result = dialog.ShowModal() == mrOK;
  if (!result)
    return false;

#endif
  value = ((DigitEntry &)dialog.GetWidget().GetWindow()).GetAngleValue();
  return true;
}
