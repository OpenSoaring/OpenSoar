// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

package org.xcsoar;

import android.util.Log;

/**
 * Diagnostic lines from the Java side into OpenSoar.log, for the
 * problems that only show on some Android devices: the map that is
 * built again and again, and USB devices such as the SteFly
 * RemoteStick that stop working or disappear while another app
 * competes for them.  The native side (Android/Main.cpp) keeps lines
 * that arrive before the log file exists and writes them later.
 */
final class DiagLog {
  private static final String TAG = "OpenSoar";

  private DiagLog() {}

  private static native void logNative(String msg);

  static void log(String msg) {
    if (Loader.loaded) {
      try {
        logNative(msg);
        return;
      } catch (UnsatisfiedLinkError e) {
        /* a native library without this function must not end the
           app over a diagnostic line */
      }
    }

    Log.i(TAG, msg);
  }
}
