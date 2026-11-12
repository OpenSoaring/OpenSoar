// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "VarioSound.hpp"
#include "Audio/Features.hpp"
#include "Audio/VarioGlue.hpp"
#include "Audio/VarioSettings.hpp"
#include "Interface.hpp"
#include "Profile/Keys.hpp"
#include "Profile/Profile.hpp"

#ifdef HAVE_VOLUME_CONTROLLER
#include "Audio/GlobalVolumeController.hpp"
#include "Audio/VolumeController.hpp"
#endif

#ifdef ANDROID
#include "Android/Main.hpp"
#include "Android/SoundUtil.hpp"
#include "Android/Context.hpp"
#endif

#include <algorithm>

static bool vario_muted = false;

#ifdef IS_OPENVARIO
#include "BackendComponents.hpp"
#include "Components.hpp"
#include "Device/Descriptor.hpp"
#include "Device/MultipleDevices.hpp"
#include "Operation/Operation.hpp"

/** the volume for variod still to be handed to the device driver */
static unsigned variod_volume;
static bool variod_pending = false;

static void
SendVariodVolume() noexcept
{
  if (!variod_pending || backend_components == nullptr ||
      backend_components->devices == nullptr)
    return;

  NullOperationEnvironment env;
  for (DeviceDescriptor *device : *backend_components->devices) {
    /* the driver keeps the volume and sends it whenever variod is
       connected, also again after variod was restarted */
    if (device->IsDriver("OpenVario") && device->GetDevice() != nullptr &&
        device->PutVolume(variod_volume, env))
      variod_pending = false;
  }
}
#endif

void
ApplyVarioSound(const VarioSoundSettings &settings) noexcept
{
  VarioSoundSettings internal = settings;

#ifdef IS_OPENVARIO
  /* with variod as the source, "Audio Vario" and the volume belong to
     variod; a volume of 0 silences it */
  if (settings.source != VarioSoundSource::INTERNAL)
    internal.enabled = false;

  variod_volume = settings.source == VarioSoundSource::OPENVARIO &&
    !vario_muted
    ? settings.volume
    : 0;
  variod_pending = true;
  SendVariodVolume();
#endif

  if (vario_muted)
    internal.enabled = false;

  AudioVarioGlue::Configure(internal);
}

void
VarioSoundTick() noexcept
{
#ifdef IS_OPENVARIO
  SendVariodVolume();
#endif
}

void
SetVarioMuted(bool muted) noexcept
{
  vario_muted = muted;
}

bool
IsVarioMuted() noexcept
{
  return vario_muted;
}

bool
IsVariodAvailable() noexcept
{
#ifdef IS_OPENVARIO
  if (backend_components == nullptr || backend_components->devices == nullptr)
    return false;

  for (const DeviceDescriptor *device : *backend_components->devices)
    if (device->IsDriver("OpenVario") && device->IsAlive())
      return true;
#endif

  return false;
}

bool
HasMasterVolume() noexcept
{
#if defined(HAVE_VOLUME_CONTROLLER) || defined(ANDROID)
  return true;
#else
  return false;
#endif
}

unsigned
GetMasterVolumeMax() noexcept
{
#ifdef HAVE_VOLUME_CONTROLLER
  return VolumeController::GetMaxValue();
#else
  return 100;
#endif
}

unsigned
GetMasterVolume() noexcept
{
#ifdef HAVE_VOLUME_CONTROLLER
  return CommonInterface::GetUISettings().sound.master_volume;
#elif defined(ANDROID)
  return std::max(SoundUtil::GetMasterVolume(Java::GetEnv(), context->Get()),
                  0);
#else
  return 0;
#endif
}

void
SetMasterVolume(unsigned percent) noexcept
{
#ifdef HAVE_VOLUME_CONTROLLER
  percent = std::min(percent, VolumeController::GetMaxValue());
  CommonInterface::SetUISettings().sound.master_volume = percent;
  Profile::Set(ProfileKeys::MasterAudioVolume, percent);
#endif

  PlayMasterVolume(percent);
}

void
PlayMasterVolume([[maybe_unused]] unsigned percent) noexcept
{
#ifdef HAVE_VOLUME_CONTROLLER
  volume_controller->SetVolume(std::min(percent,
                                        VolumeController::GetMaxValue()));
#elif defined(ANDROID)
  SoundUtil::SetMasterVolume(Java::GetEnv(), context->Get(),
                             std::min(percent, 100u));
#endif
}
