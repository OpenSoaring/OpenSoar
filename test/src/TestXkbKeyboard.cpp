// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ui/event/poll/libinput/XkbKeyboard.hpp"
#include "TestUtil.hpp"

#include <linux/input-event-codes.h>

#include <stdlib.h>

using UI::XkbKeyboard;

static unsigned
Type(XkbKeyboard &keyboard, unsigned key) noexcept
{
  const unsigned ch = keyboard.HandleKey(key, true);
  keyboard.HandleKey(key, false);
  return ch;
}

static unsigned
TypeWith(XkbKeyboard &keyboard, unsigned modifier, unsigned key) noexcept
{
  keyboard.HandleKey(modifier, true);
  const unsigned ch = Type(keyboard, key);
  keyboard.HandleKey(modifier, false);
  return ch;
}

static void
TestUS() noexcept
{
  setenv("XKB_DEFAULT_LAYOUT", "us", 1);
  XkbKeyboard keyboard;
  if (!keyboard.Open()) {
    skip(11, 1, "no xkb keymap data");
    return;
  }

  ok1(Type(keyboard, KEY_A) == 'a');
  ok1(TypeWith(keyboard, KEY_LEFTSHIFT, KEY_A) == 'A');
  ok1(TypeWith(keyboard, KEY_RIGHTSHIFT, KEY_1) == '!');
  ok1(Type(keyboard, KEY_SLASH) == '/');
  ok1(Type(keyboard, KEY_SPACE) == ' ');

  /* keys that type nothing */
  ok1(keyboard.HandleKey(KEY_LEFTSHIFT, true) == 0);
  ok1(keyboard.HandleKey(KEY_A, false) == 0);
  keyboard.HandleKey(KEY_LEFTSHIFT, false);
  ok1(Type(keyboard, KEY_UP) == 0);
  ok1(Type(keyboard, KEY_ENTER) == 0);

  /* Caps Lock stays on until it is pressed again */
  Type(keyboard, KEY_CAPSLOCK);
  ok1(Type(keyboard, KEY_B) == 'B');
  Type(keyboard, KEY_CAPSLOCK);

  /* a shift key that was held when the devices were given away does
     not stick */
  keyboard.HandleKey(KEY_LEFTSHIFT, true);
  keyboard.Reset();
  ok1(Type(keyboard, KEY_C) == 'c');
}

static void
TestGerman() noexcept
{
  setenv("XKB_DEFAULT_LAYOUT", "de", 1);
  XkbKeyboard keyboard;
  if (!keyboard.Open()) {
    skip(5, 1, "no xkb keymap data");
    return;
  }

  /* Y and Z are swapped on a German keyboard */
  ok1(Type(keyboard, KEY_Y) == 'z');
  ok1(Type(keyboard, KEY_Z) == 'y');
  ok1(TypeWith(keyboard, KEY_LEFTSHIFT, KEY_7) == '/');
  ok1(TypeWith(keyboard, KEY_RIGHTALT, KEY_Q) == '@');
  ok1(Type(keyboard, KEY_MINUS) == 0xdf); // sharp s
}

int
main()
{
  plan_tests(16);

  TestUS();
  TestGerman();

  return exit_status();
}
