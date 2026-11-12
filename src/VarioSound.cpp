// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "VarioSound.hpp"
#include "Audio/VarioGlue.hpp"
#include "Audio/VarioSettings.hpp"

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

  variod_volume = settings.source == VarioSoundSource::OPENVARIO
    ? settings.volume
    : 0;
  variod_pending = true;
  SendVariodVolume();
#endif

  AudioVarioGlue::Configure(internal);
}

void
VarioSoundTick() noexcept
{
#ifdef IS_OPENVARIO
  SendVariodVolume();
#endif
}
