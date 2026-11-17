// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Device/Port/PortName.hpp"
#include "TestUtil.hpp"

#include <string>

using std::string_view_literals::operator""sv;

static void
TestComPort()
{
  ok1(ParseComPortNumber("COM3") == 3);
  ok1(ParseComPortNumber("COM3:") == 3);
  ok1(ParseComPortNumber("\\\\.\\COM3") == 3);
  ok1(ParseComPortNumber("\\\\.\\COM12") == 12);
  ok1(ParseComPortNumber("com7") == 7);

  ok1(ParseComPortNumber("") == 0);
  ok1(ParseComPortNumber("COM") == 0);
  ok1(ParseComPortNumber("COM:") == 0);
  ok1(ParseComPortNumber("COMX") == 0);
  ok1(ParseComPortNumber("COM3x") == 0);
  ok1(ParseComPortNumber("LPT1") == 0);
  ok1(ParseComPortNumber("/dev/ttyUSB0") == 0);
}

static void
TestUevent()
{
  bool added = false;
  std::string device;

  /* a USB serial adapter is plugged in */
  ok1(ParseTtyUevent("add@/devices/platform/usb1/1-1/1-1:1.0/ttyUSB0/tty/ttyUSB0\0"
                     "ACTION=add\0"
                     "DEVPATH=/devices/platform/usb1/1-1/1-1:1.0/ttyUSB0/tty/ttyUSB0\0"
                     "SUBSYSTEM=tty\0"
                     "MAJOR=188\0"
                     "MINOR=0\0"
                     "DEVNAME=ttyUSB0\0"
                     "SEQNUM=4711\0"sv,
                     added, device));
  ok1(added);
  ok1(device == "/dev/ttyUSB0");

  /* ... and pulled out; the last string need not be terminated */
  ok1(ParseTtyUevent("remove@/devices/x/tty/ttyACM1\0"
                     "ACTION=remove\0"
                     "SUBSYSTEM=tty\0"
                     "DEVNAME=ttyACM1"sv,
                     added, device));
  ok1(!added);
  ok1(device == "/dev/ttyACM1");

  /* udev's copy of the message carries the whole path */
  ok1(ParseTtyUevent("ACTION=add\0SUBSYSTEM=tty\0DEVNAME=/dev/ttyUSB3\0"sv,
                     added, device));
  ok1(added);
  ok1(device == "/dev/ttyUSB3");

  /* the USB interface of the same adapter: not a port */
  ok1(!ParseTtyUevent("add@/devices/platform/usb1/1-1/1-1:1.0\0"
                      "ACTION=add\0"
                      "SUBSYSTEM=usb\0"
                      "DEVTYPE=usb_interface\0"sv,
                      added, device));

  /* a memory stick */
  ok1(!ParseTtyUevent("ACTION=add\0SUBSYSTEM=block\0DEVNAME=sda1\0"sv,
                      added, device));

  /* a change of a port is neither an arrival nor a departure */
  ok1(!ParseTtyUevent("ACTION=change\0SUBSYSTEM=tty\0DEVNAME=ttyUSB0\0"sv,
                      added, device));

  /* a port without a node */
  ok1(!ParseTtyUevent("ACTION=add\0SUBSYSTEM=tty\0"sv, added, device));

  ok1(!ParseTtyUevent(""sv, added, device));
}

int main()
{
  plan_tests(12 + 14);

  TestComPort();
  TestUevent();

  return exit_status();
}
