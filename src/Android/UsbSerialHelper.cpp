// SPDX-License-Identifier: GPL-2.0-only
// Copyright The XCSoar Project

#include "UsbSerialHelper.hpp"
#include "Context.hpp"
#include "NativeDetectDeviceListener.hpp"
#include "PortBridge.hpp"
#include "Components.hpp"
#include "BackendComponents.hpp"
#include "Device/MultipleDevices.hpp"
#include "Operation/PopupOperationEnvironment.hpp"
#include "java/Class.hxx"
#include "java/Env.hxx"
#include "java/String.hxx"

#include <jni.h>

static Java::TrivialClass cls;
static jmethodID ctor;
static jmethodID close_method;
static jmethodID connect_method;
static jmethodID addDetectDeviceListener_method;
static jmethodID removeDetectDeviceListener_method;

bool
UsbSerialHelper::Initialise(JNIEnv *env) noexcept
{
  assert(!cls.IsDefined());
  assert(env != nullptr);

  if (!cls.FindOptional(env, "de/opensoar/UsbSerialHelper")) {
    /* Android < 3.1 doesn't have Usb Host support */
    return false;
  }

  ctor = env->GetMethodID(cls, "<init>",
                          "(Landroid/content/Context;)V");
  if (Java::DiscardException(env)) {
    /* need to check for Java exceptions again because the first
       method lookup initializes the Java class */
    cls.Clear(env);
    return false;
  }

  close_method = env->GetMethodID(cls, "close", "()V");
  connect_method = env->GetMethodID(cls, "connect",
                                    "(Ljava/lang/String;I)Lde/opensoar/AndroidPort;");

  addDetectDeviceListener_method =
    env->GetMethodID(cls, "addDetectDeviceListener",
                     "(Lde/opensoar/DetectDeviceListener;)V");
  removeDetectDeviceListener_method =
    env->GetMethodID(cls, "removeDetectDeviceListener",
                     "(Lde/opensoar/DetectDeviceListener;)V");

  return true;
}

void
UsbSerialHelper::Deinitialise(JNIEnv *env) noexcept
{
  cls.ClearOptional(env);
}

UsbSerialHelper::UsbSerialHelper(JNIEnv *env, Context &context)
  :Java::GlobalObject(env,
                      Java::NewObjectRethrow(env, cls, ctor, context.Get()))
{
}

UsbSerialHelper::~UsbSerialHelper() noexcept
{
  Java::GetEnv()->CallVoidMethod(Get(), close_method);
}

Java::LocalObject
UsbSerialHelper::AddDetectDeviceListener(JNIEnv *env,
                                         DetectDeviceListener &_l) noexcept
{
  auto l = NativeDetectDeviceListener::Create(env, _l);
  env->CallVoidMethod(Get(), addDetectDeviceListener_method, l.Get());
  return l;
}

void
UsbSerialHelper::RemoveDetectDeviceListener(JNIEnv *env, jobject l) noexcept
{
  env->CallVoidMethod(Get(), removeDetectDeviceListener_method, l);
}

PortBridge *
UsbSerialHelper::Connect(JNIEnv *env, const char *name, unsigned baud)
{
  Java::String name2(env, name);
  auto obj = Java::CallObjectMethodRethrow(env, Get(), connect_method,
                                           name2.Get(), (int)baud);
  assert(obj);

  return new PortBridge(env, obj);
}

/*
 * Hotplug bridge from android/src/UsbSerialHelper.java.
 *
 * On USB device attach/detach, Java calls into these static native
 * methods with the port identifier (VID:PID[serial], same format
 * stored in the device config "path"). We forward to MultipleDevices,
 * which finds any DeviceDescriptor whose config.path matches and
 * reopens / closes it — mirroring the Windows WM_DEVICECHANGE handler
 * and PortMonitorLinux.
 */

static void
ForwardPortEvent(JNIEnv *env, jstring _id, bool attached) noexcept
{
  if (backend_components == nullptr ||
      backend_components->devices == nullptr ||
      _id == nullptr)
    return;

  const auto id = Java::String::GetUTFChars(env, _id);
  static PopupOperationEnvironment op_env;

  if (attached)
    backend_components->devices->DetectedPort(id.c_str(), op_env);
  else
    backend_components->devices->RemovedPort(id.c_str(), op_env);
}

extern "C" {

JNIEXPORT void JNICALL
Java_de_opensoar_UsbSerialHelper_nativeOnPortDetected(JNIEnv *env,
                                                      jclass /*cls*/,
                                                      jstring id) noexcept
{
  ForwardPortEvent(env, id, true);
}

JNIEXPORT void JNICALL
Java_de_opensoar_UsbSerialHelper_nativeOnPortRemoved(JNIEnv *env,
                                                     jclass /*cls*/,
                                                     jstring id) noexcept
{
  ForwardPortEvent(env, id, false);
}

} // extern "C"
