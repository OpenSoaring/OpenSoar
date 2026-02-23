// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

// Implementation of SoundUtil using the AVFoundation framework 
// Currently for iOS only, not macOS (AVAudioSession is iOS only)

#include "SoundUtil.hpp"
#include "LogFile.hpp"

#include <TargetConditionals.h>

#if TARGET_OS_IPHONE
#import <AVFoundation/AVFoundation.h>
#endif
#import <Foundation/Foundation.h>

#if TARGET_OS_IPHONE
static AVAudioPlayer *player = nil;
#endif

#include <string_view>

bool
SoundUtil::Play([[maybe_unused]]const std::string_view resource_name)
{
#if TARGET_OS_IPHONE
  // Map resource names to actual file names
  // ToDo: Avoid duplication of static mapping information with android/src/SoundUtil.java ?
  const char *filename = nullptr;
  if (resource_name == "IDR_FAIL") {
    filename = "fail";
  } else if (resource_name == "IDR_INSERT") {
    filename = "insert";
  } else if (resource_name == "IDR_REMOVE") {
    filename = "remove";
  } else if (resource_name == "IDR_WAV_BEEPBWEEP") {
    filename = "beep_bweep";
  } else if (resource_name == "IDR_WAV_CLEAR") {
    filename = "beep_clear";
  } else if (resource_name == "IDR_WAV_DRIP") {
    filename = "beep_drip";
  } else {
    LogFormat("Unknown sound resource: %s", resource_name.data());
    return false;
  }
  
  // Construct path to WAV file in the app bundle
  NSString *filenameStr = [NSString stringWithUTF8String:filename];
  NSString *mainBundlePath = [[NSBundle mainBundle] bundlePath];
  NSString *wavPath = [mainBundlePath stringByAppendingPathComponent:
                       [NSString stringWithFormat:@"%@.wav", filenameStr]];
  
  NSError *error = nil;
  NSURL *fileURL = [NSURL fileURLWithPath:wavPath];
  
  player = [[AVAudioPlayer alloc] initWithContentsOfURL:fileURL error:&error];

  if (!player) {
    if (error) {
      LogFormat("Failed to create AVAudioPlayer for %s (%s): %s",
                resource_name.data(), filename,
                [[error localizedDescription] UTF8String]);
    } else {
      LogFormat("Failed to create AVAudioPlayer for %s (%s): unknown reason",
                resource_name.data(), filename);
    }
    return false;
  }

  [player prepareToPlay];
  [player play];

  return true;
#else
  return false;
#endif
}
