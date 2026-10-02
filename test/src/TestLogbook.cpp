// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Logger/Logbook.hpp"
#include "TestUtil.hpp"
#include "system/FileUtil.hpp"
#include "system/Path.hpp"
#include "io/FileOutputStream.hxx"
#include "io/BufferedOutputStream.hxx"

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
  e.release = BrokenDateTime(2026, 7, 14, 10, 11, 40);
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
  e.free_speed = 93.47;
  e.log_file = "2026-07-14-XCS-AAA-01.igc";
  e.file_type = Logbook::FileTypeOf(e.log_file);
  e.recorder_code = "XCS";
  e.recorder_type = "XCSOAR,XCSOAR 7.45";
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
  /* decimal comma, as a German or most European spreadsheets expect */
  ok1(line.find(";512,3;93,5;498,7;553,3;") != line.npos);
  ok1(line.find(";2026-07-14-XCS-AAA-01.igc;IGC;XCS;XCSOAR,XCSOAR 7.45;") != line.npos);

  LogbookEntry p;
  ok1(Logbook::ParseLine(line, p));
  ok1(p.takeoff == e.takeoff);
  ok1(p.landing == e.landing);
  ok1(p.takeoff_place == e.takeoff_place);
  ok1(p.landing_place == e.landing_place);
  ok1(p.launch == e.launch);
  ok1(p.release == e.release);
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
  ok1(std::fabs(p.free_speed - 93.5) < 0.01);
  ok1(p.log_file == e.log_file);
  ok1(p.file_type == "IGC");
  ok1(p.recorder_code == "XCS");
  ok1(p.recorder_type == e.recorder_type);
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
  ok1(Logbook::ParseLine("2026-07-14;23:10:00;01:20:00;;A;B;winch;00:01:30;;;"
                         "Ka 8;D-5678;;123,4;61,2;110,5;95,2", e));
  ok1(e.launch == LogbookEntry::Launch::WINCH);
  /* the release after midnight UTC is on the next day */
  ok1(e.release == BrokenDateTime(2026, 7, 15, 0, 1, 30));
  ok1(std::fabs(e.free_distance - 123400) < 1);
  ok1(std::fabs(e.free_speed - 61.2) < 0.01);
  ok1(std::fabs(e.dmst_points - 95.2) < 0.01);
  ok1(e.max_altitude < 0);
  ok1(e.HasFlightTime());
  ok1(e.GetFlightTime() == std::chrono::minutes{130});

  /* a flight without landing (e.g. the program was stopped) */
  ok1(Logbook::ParseLine("2026-07-15;09:00:00;;;A", e));
  ok1(!e.landing.IsPlausible());
  ok1(!e.HasFlightTime());
}

static void
TestHeader()
{
  /* the header of the first version had no speed and called the log
     file "IGC file"; such a file is read by its header */
  Logbook::ColumnMap map;
  ok1(map.ParseHeader("Date;Takeoff (UTC);Landing (UTC);Duration;"
                      "Takeoff place;Landing place;Launch;Pilot;Copilot;"
                      "Aircraft;Registration;Competition ID;"
                      "Free distance (km);DMSt distance (km);DMSt points;"
                      "DMSt shape;Max. altitude (m);IGC file;Remark"));
  LogbookEntry e;
  ok1(Logbook::ParseLine("2026-07-14;10:00:00;14:00:00;4:00;A;A;aerotow;"
                         "Uwe;;LS 4;D-1234;U2;300.5;290.0;310.2;triangle;"
                         "2100;a.igc;old", e, map));
  ok1(std::fabs(e.free_distance - 300500) < 1);
  ok1(e.free_speed == 0);
  ok1(std::fabs(e.dmst_points - 310.2) < 0.01);
  ok1(e.max_altitude == 2100);
  ok1(e.log_file == "a.igc");
  ok1(e.remark == "old");

  /* columns moved in a spreadsheet */
  ok1(map.ParseHeader("Date;Takeoff (UTC);Remark;Log file"));
  ok1(Logbook::ParseLine("2026-07-14;10:00:00;moved;b.nmea", e, map));
  ok1(e.remark == "moved");
  ok1(e.log_file == "b.nmea");

  /* the current header maps to the default order */
  ok1(map.ParseHeader(Logbook::GetHeader()));
  ok1(!map.ParseHeader("2026-07-14;10:00:00"));

  ok1(Logbook::FileTypeOf("x.nmea") == "NMEA");
  ok1(Logbook::FileTypeOf("noext").empty());
}

/**
 * A flight appended to the file of an older version converts the
 * file, so its lines match the header again.
 */
static void
TestAppendToOldFile()
{
  const Path path("TestLogbook.tmp.csv");
  {
    FileOutputStream file(path);
    BufferedOutputStream writer(file);
    writer.Write("Date;Takeoff (UTC);Landing (UTC);Duration;Takeoff place;"
               "Landing place;Launch;Pilot;Copilot;Aircraft;Registration;"
               "Competition ID;Free distance (km);DMSt distance (km);"
               "DMSt points;DMSt shape;Max. altitude (m);IGC file;Remark\r\n"
               "2026-07-13;10:00:00;14:00:00;4:00;A;A;winch;Uwe;;LS 4;"
               "D-1234;U2;300.5;290.0;310.2;triangle;2100;a.igc;old\r\n");
    writer.Flush();
    file.Commit();
  }

  Logbook::Append(path, MakeEntry());

  const auto entries = Logbook::Read(path);
  ok1(entries.size() == 2);
  ok1(entries.size() == 2 && entries[0].log_file == "a.igc" &&
      std::fabs(entries[0].dmst_points - 310.2) < 0.01);
  ok1(entries.size() == 2 && entries[1].log_file == MakeEntry().log_file &&
      std::fabs(entries[1].free_speed - 93.5) < 0.01);

  File::Delete(path);
}

int
main()
{
  plan_tests(65);

  TestSplit();
  TestRoundTrip();
  TestParse();
  TestHeader();
  TestAppendToOldFile();

  return exit_status();
}
