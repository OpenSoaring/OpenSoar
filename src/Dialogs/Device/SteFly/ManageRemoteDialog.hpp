// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

class Device;
class DeviceDescriptor;

/**
 * Open the SteFly Remote Stick manage dialog.
 *
 * The dialog uses the DeviceDescriptor (not only the Device) so it
 * can release and re-acquire the borrow during a Reboot — the stick
 * disconnects from USB while rebooting and needs the PortMonitor's
 * Close / Reopen to run without conflicting with the borrow.
 */
void
ManageRemoteDialog(DeviceDescriptor &descriptor, Device &device);
