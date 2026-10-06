// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "XkbKeyboard.hpp"

#include <xkbcommon/xkbcommon.h>

namespace UI {

/**
 * xkb key codes are the Linux key codes plus 8, a heritage of X11.
 */
static constexpr xkb_keycode_t
ToXkbKeyCode(unsigned key_code) noexcept
{
  return key_code + 8;
}

/**
 * Is this a character a text field can take, not a control character
 * or something that is not a Unicode scalar value?
 */
static constexpr bool
IsPrintable(uint32_t ch) noexcept
{
  return ch >= 0x20 && ch != 0x7f &&
    (ch < 0xd800 || ch > 0xdfff) && ch <= 0x10ffff;
}

XkbKeyboard::~XkbKeyboard() noexcept
{
  if (state != nullptr)
    xkb_state_unref(state);
  if (keymap != nullptr)
    xkb_keymap_unref(keymap);
  if (context != nullptr)
    xkb_context_unref(context);
}

bool
XkbKeyboard::Open() noexcept
{
  if (state != nullptr)
    return true;

  if (context == nullptr) {
    context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (context == nullptr)
      return false;
  }

  if (keymap == nullptr) {
    /* nullptr names: the rules, model, layout, variant and options
       come from the XKB_DEFAULT_* environment variables, or from the
       built-in defaults */
    keymap = xkb_keymap_new_from_names(context, nullptr,
                                       XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (keymap == nullptr)
      return false;
  }

  state = xkb_state_new(keymap);
  return state != nullptr;
}

void
XkbKeyboard::Reset() noexcept
{
  if (state == nullptr)
    return;

  xkb_state_unref(state);
  state = xkb_state_new(keymap);
}

unsigned
XkbKeyboard::HandleKey(unsigned key_code, bool pressed) noexcept
{
  if (state == nullptr)
    return 0;

  const xkb_keycode_t xkb_key_code = ToXkbKeyCode(key_code);

  /* the character depends on the modifiers before this key changes
     them, so it is looked up first */
  const uint32_t ch = pressed
    ? xkb_state_key_get_utf32(state, xkb_key_code)
    : 0;

  xkb_state_update_key(state, xkb_key_code,
                       pressed ? XKB_KEY_DOWN : XKB_KEY_UP);

  return IsPrintable(ch) ? ch : 0;
}

} // namespace UI
