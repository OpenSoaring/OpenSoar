// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "OV/System.hpp"
#include "TestUtil.hpp"

using std::string_view_literals::operator""sv;

int
main()
{
  plan_tests(6);

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

  return exit_status();
}
