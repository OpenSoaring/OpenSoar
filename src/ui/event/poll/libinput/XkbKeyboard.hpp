// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

struct xkb_context;
struct xkb_keymap;
struct xkb_state;

namespace UI {

/**
 * Turns the Linux key codes of a keyboard into the characters they
 * type, with the keyboard layout of the system and the state of the
 * modifier keys (Shift, AltGr, Caps Lock).  libinput only reports
 * which key went down; without this, a text field could only receive
 * what a fixed table makes of a key, which is an upper case letter or
 * a digit.
 *
 * The layout comes from the environment variables that libxkbcommon
 * evaluates, XKB_DEFAULT_LAYOUT (for example "de"), XKB_DEFAULT_VARIANT
 * and so on; without them it is the US layout.
 */
class XkbKeyboard final {
  struct xkb_context *context = nullptr;
  struct xkb_keymap *keymap = nullptr;
  struct xkb_state *state = nullptr;

public:
  XkbKeyboard() noexcept = default;
  ~XkbKeyboard() noexcept;

  XkbKeyboard(const XkbKeyboard &) = delete;
  XkbKeyboard &operator=(const XkbKeyboard &) = delete;

  /**
   * Load the keymap.
   *
   * @return false if libxkbcommon or its keymap data is not
   * available; the caller then has to do without characters
   */
  bool Open() noexcept;

  bool IsOpen() const noexcept {
    return state != nullptr;
  }

  /**
   * Forget the state of the modifier keys, for example after the
   * input devices were given away for a while and a key may have been
   * released meanwhile.
   */
  void Reset() noexcept;

  /**
   * Process a key event and return the character it types.
   *
   * @param key_code the Linux key code (KEY_A, ...)
   * @param pressed true for key down, false for key up
   * @return the Unicode character, or 0 if the key does not type a
   * printable character (a modifier, a cursor key, a key up event)
   */
  unsigned HandleKey(unsigned key_code, bool pressed) noexcept;
};

} // namespace UI
