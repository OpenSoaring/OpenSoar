// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

struct VarioSoundSettings;

/**
 * Apply the vario sound settings.  The program's own synthesiser plays
 * only if it is the selected source; on the OpenVario, variod gets the
 * volume if it is the selected one and 0 otherwise, so that the two
 * vario tones never play at once on the same audio output.  While the
 * vario is muted (SetVarioMuted()), neither plays.
 */
void
ApplyVarioSound(const VarioSoundSettings &settings) noexcept;

/**
 * Called by ProcessTimer(): hands a volume for variod to the driver of
 * the OpenVario device as soon as that device is open, because the
 * settings are applied before the devices are.
 */
void
VarioSoundTick() noexcept;

/**
 * Mute the vario sound, of the program and of variod, without touching
 * the stored volume.  Spoken messages and warnings stay audible.  The
 * mute is not stored: every start begins with the vario audible, so a
 * mute forgotten on the ground cannot silence the next flight.  Call
 * ApplyVarioSound() afterwards.
 */
void
SetVarioMuted(bool muted) noexcept;

[[gnu::pure]]
bool
IsVarioMuted() noexcept;

/**
 * Is variod on the OpenVario connected and sending, so that it can be
 * chosen as the source of the vario sound?  Always false on other
 * systems.
 */
[[gnu::pure]]
bool
IsVariodAvailable() noexcept;

/**
 * Does this system have a master volume the program can set: its own
 * mixer (Linux, OpenVario) or the media volume of Android?
 */
[[gnu::const]]
bool
HasMasterVolume() noexcept;

/** the highest master volume in percent */
[[gnu::const]]
unsigned
GetMasterVolumeMax() noexcept;

unsigned
GetMasterVolume() noexcept;

/**
 * Set the master volume at once.  The program's own mixer stores it in
 * the profile; Android keeps its media volume itself.
 */
void
SetMasterVolume(unsigned percent) noexcept;

/**
 * Play at this master volume without storing it, so that a settings
 * page can let the pilot hear a value before it is saved.  It ends a
 * mute of the master volume.
 */
void
PlayMasterVolume(unsigned percent) noexcept;

/**
 * Silence the whole output, spoken messages and warnings included, or
 * bring back the volume it had.  Like the vario mute it is not stored.
 * On Android the media volume of the device is set to 0, so a program
 * that ends while muted leaves the device silent until its volume is
 * raised again.
 */
void
SetMasterMuted(bool muted) noexcept;

[[gnu::pure]]
bool
IsMasterMuted() noexcept;
