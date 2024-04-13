// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "TextEntry.hpp"
#include "DialogSettings.hpp"
#include "UIGlobals.hpp"
#include "Asset.hpp"

bool
TextEntryDialog(TCHAR *text, size_t width,
                const TCHAR *caption, AllowedCharacters accb,
                bool default_shift_state)
{
  switch (UIGlobals::GetDialogSettings().text_input_style) {
  case DialogSettings::TextInputStyle::Default:
    return TouchTextEntry(text, width, caption, accb, default_shift_state);
  case DialogSettings::TextInputStyle::Keyboard:
#if 1
    return TouchTextEntry(text, width, caption, accb, default_shift_state);
#else
    if (HasPointer())
      return TouchTextEntry(text, width, caption, accb, default_shift_state);
    else {
      KnobTextEntry(text, width, caption);
      return true;
    }
#endif
  case DialogSettings::TextInputStyle::HighScore:
    KnobTextEntry(text, width, caption);
    return true;
  }

  return false;
}
