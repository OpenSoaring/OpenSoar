// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

package org.xcsoar;

import android.view.InputDevice;
import android.view.KeyEvent;

class EventBridge {
  /**
   * There are cursor keys (a stick, a keyboard), so the focus is to be
   * shown from the start; see HasCursorKeys() in src/Asset.hpp.
   */
  public static native void onCursorKeysPresent();

  /**
   * Look for a real input device with cursor keys and report it with
   * onCursorKeysPresent().  Without this, the native side learns about
   * cursor keys only from the first one pressed, and until then no
   * dialog shows its focus - on a device with a stick (SteFlyNav) the
   * first quick menu and the power dialog showed no selected button.
   */
  static void detectCursorKeys() {
    for (int id : InputDevice.getDeviceIds()) {
      InputDevice device = InputDevice.getDevice(id);
      if (device == null || device.isVirtual())
        continue;

      if ((device.getSources() & InputDevice.SOURCE_DPAD) ==
          InputDevice.SOURCE_DPAD) {
        onCursorKeysPresent();
        return;
      }

      boolean[] keys = device.hasKeys(KeyEvent.KEYCODE_DPAD_UP,
                                      KeyEvent.KEYCODE_DPAD_DOWN);
      if (keys[0] && keys[1]) {
        onCursorKeysPresent();
        return;
      }
    }
  }

  public static native void onKeyDown(int keyCode);
  public static native void onKeyUp(int keyCode);
  public static native void onMouseDown(int x, int y);
  public static native void onMouseUp(int x, int y);
  public static native void onMouseMove(int x, int y);
  public static native void onMouseCancel();

  /**
   * A second pointer was pressed.  Coordinates are view-relative
   * pixels for the first two active pointers.
   */
  public static native void onPointerDown(int x1, int y1, int x2, int y2);

  /**
   * Two (or more) pointers are moving.  Coordinates are view-relative
   * pixels for the first two active pointers.
   */
  public static native void onPointerMove(int x1, int y1, int x2, int y2);

  public static native void onPointerUp();
}
