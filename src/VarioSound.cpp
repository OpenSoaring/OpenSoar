// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "VarioSound.hpp"
#include "Audio/Features.hpp"
#include "Audio/VarioGlue.hpp"
#include "Audio/VarioSettings.hpp"
#include "Interface.hpp"
#include "LogFile.hpp"
#include "Profile/Keys.hpp"
#include "Profile/Profile.hpp"
#include "Profile/Current.hpp"
#include "Profile/Map.hpp"

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

static bool master_muted = false;

/**
 * The master volume before the mute.  Android has no volume of its own
 * in the settings: while muted the device reports 0, and this value is
 * what comes back.
 */
static unsigned master_before_mute = 0;

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

  /* one line at the start and whenever the source or the switch
     changes, so that a silent vario can be told from the log: off,
     muted, or played by variod */
  static int logged_state = -1;
  const int state = int(settings.source) * 4 + settings.enabled * 2 +
    vario_muted;
  if (state != logged_state) {
    logged_state = state;
    LogFmt("Vario sound: {}, {}{}, volume {}%",
           settings.source == VarioSoundSource::INTERNAL
           ? "program" : "variod",
           settings.enabled || settings.source != VarioSoundSource::INTERNAL
           ? "on" : "off",
           vario_muted ? ", muted" : "",
           (unsigned)settings.volume);
  }

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
StoreVarioSoundMode(const VarioSoundSettings &settings) noexcept
{
  Profile::Set(ProfileKeys::SoundAudioVario, unsigned(settings.GetMode()));
  Profile::map.Remove(ProfileKeys::VarioSoundSource);
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

  /* alive is not enough: sensord without sensors (e.g. a test
     installation without the sensor board) sends 99 m/s, which the
     driver does not take as a vario; only a real vario shows that
     variod has something to play */
  for (const DeviceDescriptor *device : *backend_components->devices)
    if (device->IsDriver("OpenVario") && device->IsAlive() &&
        device->GetData().total_energy_vario_available)
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

bool
IsMasterVolumeInSteps() noexcept
{
#ifdef ANDROID
  return true;
#else
  return false;
#endif
}

unsigned
GetMasterVolumeMax() noexcept
{
#ifdef ANDROID
  const int steps =
    SoundUtil::GetMasterVolumeSteps(Java::GetEnv(), context->Get());
  return steps > 0 ? steps : 15;
#else
  /* VolumeController goes up to 150 percent, but above 100 the
     program's mixer amplifies and clips; the loudness belongs to the
     sound card and the speaker */
  return 100;
#endif
}

unsigned
GetMasterVolumeIncrement() noexcept
{
  /* the device's own steps, or percent in steps of 10 like the vario */
  return IsMasterVolumeInSteps() ? 1 : 10;
}

unsigned
GetMasterVolume() noexcept
{
  if (master_muted)
    return master_before_mute;

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
SetMasterVolume(unsigned level) noexcept
{
  level = std::min(level, GetMasterVolumeMax());
#ifdef HAVE_VOLUME_CONTROLLER
  CommonInterface::SetUISettings().sound.master_volume = level;
  Profile::Set(ProfileKeys::MasterAudioVolume, level);
#endif

  PlayMasterVolume(level);
}

/** set the output without looking at the mute */
static void
OutputMasterVolume([[maybe_unused]] unsigned level) noexcept
{
#ifdef HAVE_VOLUME_CONTROLLER
  volume_controller->SetVolume(std::min(level, GetMasterVolumeMax()));
#elif defined(ANDROID)
  SoundUtil::SetMasterVolume(Java::GetEnv(), context->Get(),
                             std::min(level, GetMasterVolumeMax()));
#endif
}

void
PlayMasterVolume(unsigned level) noexcept
{
  master_muted = false;
  OutputMasterVolume(level);
}

void
SetMasterMuted(bool muted) noexcept
{
  if (muted == master_muted)
    return;

  if (muted) {
    master_before_mute = GetMasterVolume();
    master_muted = true;
    OutputMasterVolume(0);
  } else {
    master_muted = false;
    OutputMasterVolume(master_before_mute);
  }
}

bool
IsMasterMuted() noexcept
{
  return master_muted;
}
