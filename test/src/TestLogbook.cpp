// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Logger/Logbook.hpp"
#include "TestUtil.hpp"

#include <cmath>
#include <string>

static LogbookEntry
MakeEntry()
{
  LogbookEntry e;
  e.takeoff = BrokenDateTime(2026, 7, 14, 10, 3, 15);
  e.landing = BrokenDateTime(2026, 7, 14, 15, 47, 2);
  e.takeoff_place = "Aalen-Elchingen";
  e.landing_place = "Field; near \"the\" river";
  e.launch = LogbookEntry::Launch::AEROTOW;
  e.pilot = "Uwe";
  e.copilot = "";
  e.aircraft = "ASW 28";
  e.registration = "D-1234";
  e.competition_id = "U2";
  e.free_distance = 512345;
  e.dmst_distance = 498700;
  e.dmst_points = 553.3;
  e.dmst_shape = LogbookStatistics::DMStShape::TRIANGLE;
  e.max_altitude = 2310;
  e.igc_file = "2026-07-14-XCS-AAA-01.igc";
  e.remark = "first line\nsecond line";
  return e;
}

static void
TestSplit()
{
  auto c = Logbook::SplitLine("a;\"b;c\";\"d\"\"e\";;f\r");
  ok1(c.size() == 5);
  ok1(c[0] == "a");
  ok1(c[1] == "b;c");
  ok1(c[2] == "d\"e");
  ok1(c[3].empty());
  ok1(c[4] == "f");
}

static void
TestRoundTrip()
{
  const LogbookEntry e = MakeEntry();
  const std::string line = Logbook::FormatLine(e);

  /* one flight is one line, whatever the remark contains */
  ok1(line.find('\n') == line.npos);
  ok1(line.starts_with("2026-07-14;10:03:15;15:47:02;5:43;"));

  LogbookEntry p;
  ok1(Logbook::ParseLine(line, p));
  ok1(p.takeoff == e.takeoff);
  ok1(p.landing == e.landing);
  ok1(p.takeoff_place == e.takeoff_place);
  ok1(p.landing_place == e.landing_place);
  ok1(p.launch == e.launch);
  ok1(p.pilot == e.pilot);
  ok1(p.aircraft == e.aircraft);
  ok1(p.registration == e.registration);
  ok1(p.competition_id == e.competition_id);
  /* the file keeps one decimal of a kilometre */
  ok1(std::fabs(p.free_distance - 512300) < 1);
  ok1(std::fabs(p.dmst_distance - 498700) < 1);
  ok1(std::fabs(p.dmst_points - 553.3) < 0.01);
  ok1(p.dmst_shape == e.dmst_shape);
  ok1(p.max_altitude == 2310);
  ok1(p.igc_file == e.igc_file);
  ok1(p.remark == "first line second line");
}

static void
TestParse()
{
  LogbookEntry e;

  /* the header is not a flight */
  ok1(!Logbook::ParseLine(Logbook::GetHeader(), e));
  ok1(!Logbook::ParseLine("", e));

  /* saved by a spreadsheet in a German locale: decimal comma, the
     empty columns at the end dropped; across midnight UTC */
  ok1(Logbook::ParseLine("2026-07-14;23:10:00;01:20:00;;A;B;winch;;;"
                         "Ka 8;D-5678;;123,4;110,5;95,2", e));
  ok1(e.launch == LogbookEntry::Launch::WINCH);
  ok1(std::fabs(e.free_distance - 123400) < 1);
  ok1(std::fabs(e.dmst_points - 95.2) < 0.01);
  ok1(e.max_altitude < 0);
  ok1(e.HasFlightTime());
  ok1(e.GetFlightTime() == std::chrono::minutes{130});

  /* a flight without landing (e.g. the program was stopped) */
  ok1(Logbook::ParseLine("2026-07-15;09:00:00;;;A", e));
  ok1(!e.landing.IsPlausible());
  ok1(!e.HasFlightTime());
}

int
main()
{
  plan_tests(37);

  TestSplit();
  TestRoundTrip();
  TestParse();

  return exit_status();
}
