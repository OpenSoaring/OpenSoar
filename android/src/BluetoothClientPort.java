// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

package de.opensoar;

import java.io.IOException;
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

    if (closed)
      return "Bluetooth connect cancelled";

    try {
      BluetoothSocket socket =
        connect(device.createRfcommSocketToServiceRecord(uuid));
      setPort(new BluetoothPort(socket));
      return null;
    } catch (Exception e) {
      Log.e(TAG, "Failed to connect to Bluetooth", e);
      return e.getClass().getSimpleName() + ": " + e.getMessage();
    }
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
