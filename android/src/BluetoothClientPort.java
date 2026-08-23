// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

package de.opensoar;

import java.io.IOException;
import java.lang.reflect.Method;
import java.util.UUID;
import android.util.Log;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;

/**
 * An #AndroidPort implementation that initiates a Bluetooth RFCOMM
 * connection.
 */
class BluetoothClientPort extends ProxyAndroidPort implements Runnable {
  private static final String TAG = "OpenSoar";

  /**
   * How many connect attempts (with different socket flavours) shall
   * be made before giving up?
   */
  private static final int MAX_ATTEMPTS = 3;

  /**
   * Delay between two connect attempts; the Android Bluetooth stack
   * needs some time to tear down the previous (failed) attempt.
   */
  private static final long RETRY_DELAY_MS = 1500;

  /**
   * If a connect attempt fails only after this time, the remote device
   * did not answer at all (page timeout).  Another socket flavour
   * cannot help then, and would block the Bluetooth radio for another
   * few seconds - which matters because other Bluetooth devices are
   * supposed to keep working meanwhile.
   */
  private static final long NO_ANSWER_THRESHOLD_MS = 4000;

  /**
   * Names of the connect attempts, for the error message.
   */
  private static final String[] ATTEMPT_NAMES = {
    "secure/SDP", "insecure/SDP", "channel 1",
  };

  private final BluetoothAdapter adapter;
  private final BluetoothDevice device;
  private final UUID uuid;

  /**
   * The socket of the connect attempt which is currently in progress.
   * Used by close() to abort a blocking connect().
   */
  private volatile BluetoothSocket socket;

  /**
   * Is run() still trying to establish the connection?
   */
  private volatile boolean connecting = true;

  /**
   * Has close() been called?
   */
  private volatile boolean closed = false;

  private Thread thread;

  BluetoothClientPort(BluetoothAdapter _adapter, BluetoothDevice _device,
                      UUID _uuid)
    throws IOException {
    adapter = _adapter;
    device = _device;
    uuid = _uuid;

    thread = new Thread(this, toString());
    thread.start();
  }

  @Override public String toString() {
    return "Bluetooth " + BluetoothHelper.getDisplayString(device);
  }

  @Override public void close() {
    closed = true;

    /* closing the socket is the documented way to abort a connect()
       which is in progress */
    BluetoothSocket socket = this.socket;
    if (socket != null) {
      try {
        socket.close();
      } catch (IOException e) {
        Log.w(TAG, "Failed to close BluetoothSocket", e);
      }
    }

    /* ensure that run() has finished before calling
       ProxyAndroidPort.close() */
    Thread thread = this.thread;
    if (thread != null) {
      try {
        thread.join();
      } catch (InterruptedException e) {
      }
      this.thread = null;
    }

    super.close();
  }

  @Override public int getState() {
    return connecting
      ? STATE_LIMBO
      : super.getState();
  }

  /**
   * Connect the given socket.  On failure, the socket is always
   * closed; otherwise the Bluetooth stack keeps a half-open RFCOMM
   * socket which makes all following connect attempts fail.
   */
  private BluetoothSocket connect(BluetoothSocket socket) throws IOException {
    this.socket = socket;

    try {
      socket.connect();
      return socket;
    } catch (IOException e) {
      try {
        socket.close();
      } catch (IOException e2) {
        Log.w(TAG, "Failed to close BluetoothSocket", e2);
      }

      throw e;
    } finally {
      this.socket = null;
    }
  }

  /**
   * Last resort: the hidden createRfcommSocket() method connects to a
   * fixed RFCOMM channel without querying SDP.  Many serial adapters
   * only work this way.
   */
  private BluetoothSocket createFallbackSocket() throws Exception {
    Method m = device.getClass().getMethod("createRfcommSocket", int.class);
    return (BluetoothSocket)m.invoke(device, Integer.valueOf(1));
  }

  private BluetoothSocket createSocket(int attempt) throws Exception {
    switch (attempt) {
    case 0:
      return device.createRfcommSocketToServiceRecord(uuid);

    case 1:
      /* some adapters refuse the authenticated/encrypted link */
      return device.createInsecureRfcommSocketToServiceRecord(uuid);

    default:
      return createFallbackSocket();
    }
  }

  /**
   * @return null on success, an error message on failure
   */
  private String connectLoop() {
    /* Android documentation: discovery is a heavyweight procedure,
       and it will slow down or break a connection attempt */
    try {
      if (adapter.isDiscovering())
        adapter.cancelDiscovery();
    } catch (SecurityException e) {
      Log.w(TAG, "cancelDiscovery() failed", e);
    }

    StringBuilder errors = null;

    for (int attempt = 0; attempt < MAX_ATTEMPTS && !closed; ++attempt) {
      if (attempt > 0) {
        try {
          Thread.sleep(RETRY_DELAY_MS);
        } catch (InterruptedException e) {
          break;
        }

        if (closed)
          break;
      }

      final long start = System.nanoTime();

      try {
        BluetoothSocket socket = connect(createSocket(attempt));
        setPort(new BluetoothPort(socket));
        return null;
      } catch (Exception e) {
        final long elapsed_ms = (System.nanoTime() - start) / 1000000;

        Log.e(TAG, "Bluetooth connect attempt " + (attempt + 1) + " of "
              + MAX_ATTEMPTS + " failed after " + elapsed_ms + " ms", e);

        /* collect all attempts: the first one (SDP) can fail for a
           different reason than the last one (fixed channel), and
           only seeing the last message hides that */
        if (errors == null)
          errors = new StringBuilder();
        else
          errors.append(" | ");

        errors.append(ATTEMPT_NAMES[Math.min(attempt, ATTEMPT_NAMES.length - 1)])
          .append(" after ").append(elapsed_ms).append(" ms: ")
          .append(e.getClass().getSimpleName())
          .append(": ").append(e.getMessage());

        if (elapsed_ms >= NO_ANSWER_THRESHOLD_MS) {
          /* out of range or switched off - do not occupy the radio
             with the remaining flavours */
          errors.append(" (no answer, skipping the remaining attempts)");
          break;
        }
      }
    }

    return errors != null
      ? errors.toString()
      : "Bluetooth connect cancelled";
  }

  @Override public void run() {
    String error;

    try {
      error = connectLoop();
    } catch (Throwable t) {
      Log.e(TAG, "Failed to connect to Bluetooth", t);
      error = t.toString();
    } finally {
      socket = null;
      connecting = false;
    }

    if (error != null && !closed)
      error(error);

    stateChanged();
  }
}
