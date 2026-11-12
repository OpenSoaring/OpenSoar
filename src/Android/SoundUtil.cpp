// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "SoundUtil.hpp"
#include "java/Class.hxx"
#include "java/String.hxx"
#include "LocalPath.hpp"
#include "system/Path.hpp"

#include <string>

namespace SoundUtil {
static Java::TrivialClass cls;
static jmethodID play_method;
static jmethodID playExternal_method;
static jmethodID playSequence_method;
static jmethodID getMasterVolumeSteps_method;
static jmethodID getMasterVolume_method;
static jmethodID setMasterVolume_method;
} // namespace SoundUtil

void
SoundUtil::Initialise(JNIEnv *env)
{
  assert(!cls.IsDefined());
  assert(env != nullptr);

  cls.Find(env, "org/xcsoar/SoundUtil");
  play_method = env->GetStaticMethodID(cls, "play",
                                       "(Landroid/content/Context;"
                                       "Ljava/lang/String;)Z");
  playExternal_method = env->GetStaticMethodID(cls, "playExternal",
                                               "(Landroid/content/Context;"
                                               "Ljava/lang/String;)Z");
  playSequence_method = env->GetStaticMethodID(cls, "playSequence",
                                               "(Landroid/content/Context;"
                                               "Ljava/lang/String;)Z");
  getMasterVolumeSteps_method =
    env->GetStaticMethodID(cls, "getMasterVolumeSteps",
                           "(Landroid/content/Context;)I");
  getMasterVolume_method = env->GetStaticMethodID(cls, "getMasterVolume",
                                                  "(Landroid/content/Context;)I");
  setMasterVolume_method = env->GetStaticMethodID(cls, "setMasterVolume",
                                                  "(Landroid/content/Context;I)V");
}

void
SoundUtil::Deinitialise(JNIEnv *env)
{
  cls.Clear(env);
}

bool
SoundUtil::Play(JNIEnv *env, jobject context, const char *name)
{
  Java::String paramName(env, name);
  return env->CallStaticBooleanMethod(cls, play_method, context,
                                      paramName.Get());
}

bool
SoundUtil::PlayExternal(JNIEnv *env, jobject context, const char *path)
{
  AllocatedPath absolutePath = LocalPath(path);
  Java::String paramName(env, absolutePath.c_str());
  return env->CallStaticBooleanMethod(cls, playExternal_method, context,
                                      paramName.Get());
}

bool
SoundUtil::PlaySequence(JNIEnv *env, jobject context,
                        std::span<const char *const> names)
{
  /* the names are passed as one string, separated by spaces */
  std::string joined;
  for (const char *name : names) {
    if (!joined.empty())
      joined.push_back(' ');
    joined.append(name);
  }

  Java::String paramNames(env, joined.c_str());
  return env->CallStaticBooleanMethod(cls, playSequence_method, context,
                                      paramNames.Get());
}

int
SoundUtil::GetMasterVolumeSteps(JNIEnv *env, jobject context)
{
  return env->CallStaticIntMethod(cls, getMasterVolumeSteps_method, context);
}

int
SoundUtil::GetMasterVolume(JNIEnv *env, jobject context)
{
  return env->CallStaticIntMethod(cls, getMasterVolume_method, context);
}

void
SoundUtil::SetMasterVolume(JNIEnv *env, jobject context, unsigned step)
{
  env->CallStaticVoidMethod(cls, setMasterVolume_method, context,
                            (jint)step);
}
