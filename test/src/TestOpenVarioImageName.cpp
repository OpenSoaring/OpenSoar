// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "OV/System.hpp"
#include "TestUtil.hpp"

using std::string_view_literals::operator""sv;

int
main()
{
  plan_tests(12);

  /* the first line of /boot/image-version-info as the image build
     writes it */
  ok1(OpenvarioImageName("OV-3.2.20-CB2-CH57-openvario-image.img"sv) ==
      "OV-3.2.20-CB2-CH57-openvario-image"sv);

  /* the name of a downloaded, still compressed image */
  ok1(OpenvarioImageName("OV-3.2.20-CB2-CH57.img.gz"sv) ==
      "OV-3.2.20-CB2-CH57"sv);

  ok1(OpenvarioImageName("OV-3.2.20-CB2-CH57"sv) ==
      "OV-3.2.20-CB2-CH57"sv);

  /* only the suffixes are removed, not the same letters inside */
  ok1(OpenvarioImageName("OV.img-test"sv) == "OV.img-test"sv);

  /* a bare suffix is not a name; leave it alone rather than return
     an empty string */
  ok1(OpenvarioImageName(".img"sv) == ".img"sv);
  ok1(OpenvarioImageName(""sv).empty());

  /* the systemd automount unit alone, no stick plugged in */
  ok1(!HasRealMount("systemd-1 /usb/usbstick autofs rw,relatime 0 0\n"sv,
                    "/usb/usbstick"sv));

  /* the stick mounted on top of the automount point */
  ok1(HasRealMount("systemd-1 /usb/usbstick autofs rw,relatime 0 0\n"
                   "/dev/sda1 /usb/usbstick vfat rw,sync 0 0\n"sv,
                   "/usb/usbstick"sv));

  /* a single line without a trailing newline, as the line reader
     hands it over */
  ok1(HasRealMount("/dev/sda1 /usb/usbstick vfat rw,sync 0 0"sv,
                   "/usb/usbstick"sv));

  /* other mount points, and a longer path that only starts alike */
  ok1(!HasRealMount("/dev/mmcblk0p1 /boot vfat rw 0 0\n"
                    "/dev/sda1 /usb/usbstick2 vfat rw 0 0\n"sv,
                    "/usb/usbstick"sv));

  ok1(!HasRealMount(""sv, "/usb/usbstick"sv));
  ok1(!HasRealMount("broken-line\n"sv, "/usb/usbstick"sv));

  return exit_status();
}
