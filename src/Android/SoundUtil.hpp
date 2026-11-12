// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <jni.h>

#include <span>

namespace SoundUtil {

void Initialise(JNIEnv *env);
void Deinitialise(JNIEnv *env);

bool Play(JNIEnv *env, jobject context, const char *name);
bool PlayExternal(JNIEnv *env, jobject context, const char *path);

/**
 * Play the bundled sounds one after another, replacing a sequence
 * which is still playing.
 */
bool PlaySequence(JNIEnv *env, jobject context,
                  std::span<const char *const> names);

/**
 * The number of steps of the Android media volume (often 15), or -1.
 */
int GetMasterVolumeSteps(JNIEnv *env, jobject context);

/**
 * The step of the Android media volume, between 0 and
 * GetMasterVolumeSteps(), or -1.
 */
int GetMasterVolume(JNIEnv *env, jobject context);

/**
 * Set the Android media volume to this step, and the notification
 * volume to the same share of its own steps.
 */
void SetMasterVolume(JNIEnv *env, jobject context, unsigned step);

} // namespace SoundUtil
