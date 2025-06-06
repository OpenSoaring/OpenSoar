// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Weather/METARParser.hpp"
#include "Weather/METAR.hpp"
#include "Weather/ParsedMETAR.hpp"
#include "Units/System.hpp"
#include "TestUtil.hpp"


int
main()
{
  plan_tests(49);

  METAR metar;

  {
    ParsedMETAR parsed;
    metar.content = "EDDL 231050Z 31007MPS 9999 FEW020 SCT130 23/18 Q1015 NOSIG";
    metar.decoded = "";
    if (!ok1(METARParser::Parse(metar, parsed)))
      return exit_status();

    ok1(parsed.icao_code == "EDDL");
    ok1(parsed.day_of_month == 23);
    ok1(parsed.hour == 10);
    ok1(parsed.minute == 50);
    ok1(parsed.qnh_available);
    ok1(equals(parsed.qnh.GetHectoPascal(), 1015));
    ok1(parsed.wind_available);
    ok1(equals(parsed.wind.norm, 7));
    ok1(equals(parsed.wind.bearing, 310));
    ok1(parsed.temperatures_available);
    ok1(equals(parsed.temperature, Units::ToSysUnit(23, Unit::DEGREES_CELCIUS)));
    ok1(equals(parsed.dew_point, Units::ToSysUnit(18, Unit::DEGREES_CELCIUS)));
    ok1(parsed.visibility_available);
    ok1(parsed.visibility == 9999);
    ok1(!parsed.cavok);
    ok1(!parsed.location_available);
  }
  {
    ParsedMETAR parsed;
    metar.content = "METAR KTTN 051853Z 04011KT 1/2SM VCTS SN FZFG BKN003 OVC010 M02/M02 A3006 RMK AO2 TSB40 SLP176 P0002 T10171017=";
    metar.decoded = "Pudahuel, Chile (SCEL) 33-23S 070-47W 476M\r\n"
                       "Nov 04, 2011 - 07:50 PM EDT / 2011.11.04 2350 UTC\r\n";
    if (!ok1(METARParser::Parse(metar, parsed)))
      return exit_status();

    ok1(parsed.icao_code == "KTTN");
    ok1(parsed.day_of_month == 5);
    ok1(parsed.hour == 18);
    ok1(parsed.minute == 53);
    ok1(parsed.qnh_available);
    ok1(equals(parsed.qnh.GetHectoPascal(),
               Units::ToSysUnit(30.06, Unit::INCH_MERCURY)));
    ok1(parsed.wind_available);
    ok1(equals(parsed.wind.norm, Units::ToSysUnit(11, Unit::KNOTS)));
    ok1(equals(parsed.wind.bearing, 40));
    ok1(parsed.temperatures_available);
    ok1(equals(parsed.temperature, Units::ToSysUnit(-1.7, Unit::DEGREES_CELCIUS)));
    ok1(equals(parsed.dew_point, Units::ToSysUnit(-1.7, Unit::DEGREES_CELCIUS)));
    ok1(!parsed.visibility_available);
    ok1(!parsed.cavok);
    ok1(parsed.location_available);
    ok1(equals(parsed.location, -33.3833333333333, -70.78333333333333));
  }
  {
    ParsedMETAR parsed;
    metar.content = "METAR EDJA 231950Z VRB01KT CAVOK 21/17 Q1017=";
    metar.decoded = "Duesseldorf, Germany (EDDL) 51-18N 006-46E 41M\r\n"
                       "Nov 04, 2011 - 07:50 PM EDT / 2011.11.04 2350 UTC\r\n";
    if (!ok1(METARParser::Parse(metar, parsed)))
      return exit_status();

    ok1(parsed.icao_code == "EDJA");
    ok1(parsed.day_of_month == 23);
    ok1(parsed.hour == 19);
    ok1(parsed.minute == 50);
    ok1(parsed.qnh_available);
    ok1(equals(parsed.qnh.GetHectoPascal(), 1017));
    ok1(!parsed.wind_available);
    ok1(parsed.temperatures_available);
    ok1(equals(parsed.temperature, Units::ToSysUnit(21, Unit::DEGREES_CELCIUS)));
    ok1(equals(parsed.dew_point, Units::ToSysUnit(17, Unit::DEGREES_CELCIUS)));
    ok1(!parsed.visibility_available);
    ok1(parsed.cavok);
    ok1(parsed.location_available);
    ok1(equals(parsed.location, 51.3, 6.766666666666667));
  }

  return exit_status();
}
