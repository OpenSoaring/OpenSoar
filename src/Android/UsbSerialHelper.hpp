// SPDX-License-Identifier: GPL-2.0-only
// Copyright The XCSoar Project

#pragma once

#include "java/Object.hxx"

#include <optional>
#include <string>

class Context;
class PortBridge;
class DetectDeviceListener;

class UsbSerialHelper final : protected Java::GlobalObject {
public:
  /**
   * Global initialisation.  Looks up the methods of the
   * UsbSerialHelper Java class.
   */
  static bool Initialise(JNIEnv *env) noexcept;
  static void Deinitialise(JNIEnv *env) noexcept;

  UsbSerialHelper(JNIEnv *env, Context &context);
  ~UsbSerialHelper() noexcept;

  /**
   * Start scanning for USB serial devices.  Call
   * RemoveDetectDeviceListener() with the returned value when you're
   * done.
   */
  Java::LocalObject AddDetectDeviceListener(JNIEnv *env,
                                            DetectDeviceListener &l) noexcept;

  /**
   * Stop scanning for USB serial devices.
   *
   * @param l the return value of AddDetectDeviceListener()
   */
  void RemoveDetectDeviceListener(JNIEnv *env, jobject l) noexcept;

  PortBridge *Connect(JNIEnv *env, const char *name, unsigned baud);

  /**
   * Look up an attached USB serial interface by USB vendor and
   * product id.
   *
   * @return the port id as accepted by Connect() (e.g.
   * "1209:8500#1"), or std::nullopt if no such device is attached
   */
  std::optional<std::string> FindPortId(JNIEnv *env, unsigned vendor_id,
                                        unsigned product_id) noexcept;
};
