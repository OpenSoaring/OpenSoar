// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "java/Object.hxx"
#include <jni.h>

#include <string>

class Context;
class SensorListener;
class DetectDeviceListener;
class PortBridge;

class BluetoothHelper final : protected Java::GlobalObject {
public:
  /**
   * Global initialisation.  Looks up the methods of the
   * BluetoothHelper Java class.
   */
  static bool Initialise(JNIEnv *env) noexcept;
  static void Deinitialise(JNIEnv *env) noexcept;

  BluetoothHelper(JNIEnv *env, Context &context,
                  jobject permission_manager);

  /**
   * Is the default Bluetooth adapter enabled in the Android Bluetooth
   * settings?
   */
  [[gnu::pure]]
  bool IsEnabled(JNIEnv *env) const noexcept;

  /**
   * Are all runtime permissions needed for the Bluetooth device list
   * granted?
   */
  [[gnu::pure]]
  bool HasPermissions(JNIEnv *env) const noexcept;

  enum class PermissionResult {
    /** all permissions are granted */
    GRANTED,

    /** the user is being asked right now */
    REQUESTED,

    /**
     * The user has denied the permission permanently; Android will
     * not show the dialog again, only ShowAppSettings() can help.
     */
    DENIED_PERMANENTLY,
  };

  /**
   * Ask the user for the missing Bluetooth permissions.
   */
  PermissionResult RequestPermissions(JNIEnv *env) noexcept;

  /**
   * Open the Android app settings page so the user can grant a
   * permission which was denied permanently.
   */
  void ShowAppSettings(JNIEnv *env) noexcept;

  /**
   * Returns a human-readable summary of the Bluetooth state
   * (permissions, adapter, number of bonded devices) for the log
   * file.
   */
  std::string GetDiagnostics(JNIEnv *env) const noexcept;

  [[gnu::pure]]
  const char *GetNameFromAddress(JNIEnv *env,
                                 const char *address) const noexcept;

  /**
   * Does the device support Bluetooth LE?
   */
  [[gnu::const]]
  bool HasLe(JNIEnv *env) const noexcept;

  /**
   * Start scanning for Bluetooth devices.  Call
   * RemoveDetectDeviceListener() with the returned value when you're
   * done.
   */
  Java::LocalObject AddDetectDeviceListener(JNIEnv *env,
                                            DetectDeviceListener &l) noexcept;

  /**
   * Stop scanning for Bluetooth devices.
   *
   * @param l the return value of AddDetectDeviceListener()
   */
  void RemoveDetectDeviceListener(JNIEnv *env, jobject l) noexcept;

  Java::LocalObject connectSensor(JNIEnv *env, const char *address,
                                  SensorListener &listener);

  PortBridge *connect(JNIEnv *env, const char *address);

  PortBridge *connectHM10(JNIEnv *env, const char *address);

  PortBridge *createServer(JNIEnv *env);
};
