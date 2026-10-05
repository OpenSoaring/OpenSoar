// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Logger/LogbookBuilder.hpp"
#include "TestUtil.hpp"
#include "Operation/Operation.hpp"
#include "system/FileUtil.hpp"
#include "system/Path.hpp"
#include "io/FileOutputStream.hxx"
#include "io/BufferedOutputStream.hxx"

#include <cstdio>
#include <string>

static LogbookEntry
MakeEntry(unsigned takeoff_hour, unsigned landing_hour,
          const char *log_file, const char *recorder_code = "")
{
  LogbookEntry e;
  e.takeoff = BrokenDateTime(2026, 7, 14, takeoff_hour, 0, 0);
  e.landing = BrokenDateTime(2026, 7, 14, landing_hour, 0, 0);
  e.log_file = log_file;
  e.file_type = Logbook::FileTypeOf(e.log_file);
  e.recorder_code = recorder_code;
  return e;
}

static void
TestIsComplete()
{
  LogbookEntry e = MakeEntry(10, 15, "a.igc");
  ok1(Logbook::IsComplete(e));

  /* a logger stops a minute after the landing; that is no gap */
  e.remark = "landing not confirmed, end of file";
  ok1(Logbook::IsComplete(e));

  e.remark = "2 engine runs in flight, best of 3 parts scored";
  ok1(Logbook::IsComplete(e));

  e.remark = "recording began in flight";
  ok1(!Logbook::IsComplete(e));

  e.remark = "recording ended in flight";
  ok1(!Logbook::IsComplete(e));
}

static void
TestRank()
{
  const auto own = MakeEntry(10, 15, "x.igc", "XCS");
  const auto other = MakeEntry(10, 15, "y.igc", "LXV");
  const auto nmea = MakeEntry(10, 15, "z.nmea");

  const Logbook::RecorderList none;
  ok1(Logbook::GetRank(own, none) > Logbook::GetRank(other, none));
  ok1(Logbook::GetRank(other, none) > Logbook::GetRank(nmea, none));

  /* Recorder 1 before Recorder 2 before the own IGC file */
  auto nano = MakeEntry(10, 15, "n.igc", "LXV");
  nano.recorder_serial = "3MN";
  auto flarm = MakeEntry(10, 15, "f.igc", "FLA");
  flarm.recorder_serial = "85H";
  const Logbook::RecorderList recorders{
    Logbook::NormalizeRecorder("flA 85h"),
    Logbook::NormalizeRecorder("LXV3MN"),
  };
  ok1(recorders[0] == "FLA85H");
  ok1(Logbook::GetRank(flarm, recorders) > Logbook::GetRank(nano, recorders));
  ok1(Logbook::GetRank(nano, recorders) > Logbook::GetRank(own, recorders));
  /* the same manufacturer with another serial number is another logger */
  ok1(Logbook::GetRank(other, recorders) < Logbook::GetRank(own, recorders));
}

static void
TestMergeEntry()
{
  std::vector<LogbookEntry> entries;

  /* an NMEA log first; the pilot corrected the crew by hand */
  auto nmea = MakeEntry(10, 15, "z.nmea");
  nmea.pilot = "Hand";
  ok1(Logbook::MergeEntry(entries, nmea, {}));
  ok1(entries.size() == 1);

  /* another flight of the same day */
  ok1(Logbook::MergeEntry(entries, MakeEntry(16, 17, "b.igc", "LXV"), {}));
  ok1(entries.size() == 2);

  /* the IGC file of the first flight replaces the NMEA log, the
     corrected crew stays, the aircraft comes from the IGC file */
  auto igc = MakeEntry(10, 15, "a.igc", "LXV");
  igc.pilot = "Header";
  igc.aircraft = "JS 1";
  ok1(Logbook::MergeEntry(entries, igc, {}));
  ok1(entries.size() == 2);
  ok1(entries[0].log_file == "a.igc");
  ok1(entries[0].pilot == "Hand");
  ok1(entries[0].aircraft == "JS 1");

  /* a fragment after a restart in flight does not replace the whole
     flight, but fills an empty field */
  auto fragment = MakeEntry(12, 15, "c.igc", "XCS");
  fragment.remark = "recording began in flight";
  fragment.registration = "ZS-GCG";
  ok1(!Logbook::MergeEntry(entries, fragment, {}));
  ok1(entries.size() == 2);
  ok1(entries[0].log_file == "a.igc");
  ok1(entries[0].registration == "ZS-GCG");

  /* this program's own IGC file of the same length wins */
  ok1(Logbook::MergeEntry(entries, MakeEntry(10, 15, "d.igc", "XCS"), {}));
  ok1(entries[0].log_file == "d.igc");

  /* but Recorder 1 wins over it */
  auto nano = MakeEntry(10, 15, "e.igc", "LXV");
  nano.recorder_serial = "3MN";
  ok1(!Logbook::MergeEntry(entries, nano, {}));
  ok1(Logbook::MergeEntry(entries, nano, {"LXV3MN"}));
  ok1(entries[0].log_file == "e.igc");
}

static Logbook::Recording
MakeRecording(unsigned takeoff_hour, unsigned landing_hour,
              const char *log_file, const char *recorder_code,
              double longitude)
{
  Logbook::Recording r;
  r.entry = MakeEntry(takeoff_hour, landing_hour, log_file, recorder_code);
  r.source = log_file;
  for (auto t = r.Begin(); t <= r.End(); t += std::chrono::minutes{1})
    r.track.push_back({std::chrono::floor<std::chrono::minutes>(t),
                       GeoPoint(Angle::Degrees(longitude),
                                Angle::Degrees(50))});
  return r;
}

static void
TestJoin()
{
  std::vector<Logbook::Recording> recordings;
  recordings.push_back(MakeRecording(10, 15, "nano.igc", "LXV", 10));
  recordings.push_back(MakeRecording(10, 15, "own.igc", "XCS", 10));
  /* a club mate in the air at the same time, 50 km away */
  recordings.push_back(MakeRecording(10, 14, "mate.igc", "FLA", 10.7));

  const auto flights = Logbook::JoinRecordings(std::move(recordings), {});
  ok1(flights.size() == 2);
  ok1(flights.size() == 2 && flights[0].kept.entry.log_file == "own.igc");
  ok1(flights.size() == 2 && flights[0].others.size() == 1 &&
      flights[0].others[0] == "nano.igc");
}

static void
TestIgcHeader()
{
  const Path path("TestLogbookBuilder.tmp.igc");
  {
    FileOutputStream file(path);
    BufferedOutputStream writer(file);
    writer.Write("AFLA85H\r\n"
                 "HFPLTPILOTINCHARGE:Augustin\r\n"
                 "HFCM2CREW2:undefined\r\n"
                 "HFGTYGLIDERTYPE:JS 1\r\n"
                 "HFGIDGLIDERID:ZS-GCG\r\n"
                 "HFCIDCOMPETITIONID:ZP\r\n"
                 "B1000005058000N01354000EA0040000400\r\n");
    writer.Flush();
    file.Commit();
  }

  const auto header = Logbook::ReadIgcHeader(path);
  ok1(header.recorder_code == "FLA");
  ok1(header.recorder_serial == "85H");
  ok1(header.pilot == "Augustin");
  /* "undefined" is what the logger writes for a missing value */
  ok1(header.copilot.empty());
  ok1(header.aircraft == "JS 1");
  ok1(header.registration == "ZS-GCG");

  File::Delete(path);
}

/**
 * Write an IGC file of a short flight that ends 20 seconds after the
 * landing, before the program would confirm it: one fix per second,
 * 60 seconds on the ground, 10 minutes eastwards at 30 m/s, 20 seconds
 * rolling out, 20 seconds at rest.
 */
static void
WriteShortFlight(Path path)
{
  FileOutputStream file(path);
  BufferedOutputStream writer(file);
  writer.Write("AFLA85H\r\nHFDTEDATE:140726\r\n");

  double lon_m = 0, speed = 0;
  int alt = 300;
  const unsigned ground = 60, air = 600, roll = 20, rest = 20;
  for (unsigned t = 0; t < ground + air + roll + rest; ++t) {
    if (t < ground)
      speed = 0;
    else if (t < ground + air)
      speed = 30;
    else if (t < ground + air + roll)
      speed = 30.0 * (ground + air + roll - t) / roll;
    else
      speed = 0;

    if (t >= ground && t < ground + air)
      alt = 300 + (t - ground < air / 2 ? (int)(t - ground) : (int)(ground + air - t));
    else
      alt = 300;

    lon_m += speed;
    /* 1 minute of longitude at 50 degrees is about 1192 m */
    const double minutes = lon_m / 1192.;
    const unsigned whole = (unsigned)minutes;
    const unsigned thousandths = (unsigned)((minutes - whole) * 1000);
    const unsigned secs = 10 * 3600 + t;
    char line[64];
    snprintf(line, sizeof(line),
             "B%02u%02u%02u5000000N010%02u%03uEA%05d%05d\r\n",
             secs / 3600, secs / 60 % 60, secs % 60,
             whole, thousandths, alt, alt);
    writer.Write(line);
  }

  writer.Flush();
  file.Commit();
}

static void
TestLandingAtRest()
{
  const Path path("TestLogbookBuilder.tmp.igc");
  WriteShortFlight(path);

  const auto result = Logbook::ReadFile(path, {});
  ok1(result.flights.size() == 1);
  if (result.flights.size() == 1) {
    const auto &e = result.flights.front().entry;
    /* the first fix slower than 10 km/h at the end of the roll
       (10:11:18 to 10:11:20, the positions are rounded to about a
       metre), not the last fix of the file at 10:11:39 */
    ok1(!(e.landing < BrokenDateTime(2026, 7, 14, 10, 11, 17)) &&
        !(BrokenDateTime(2026, 7, 14, 10, 11, 20) < e.landing));
    /* the logger stopped recording before the landing was confirmed;
       that is no gap */
    ok1(e.remark.empty());
    ok1(Logbook::IsComplete(e));
  } else {
    ok1(false);
    ok1(false);
    ok1(false);
  }

  File::Delete(path);
}

static void
TestRetire()
{
  const Path book("TestLogbookBuilder.tmp.csv");
  const Path index("TestLogbookBuilder.tmp.txt");
  const Path old1("logbook-2026-07-14.csv");
  const Path old2("logbook-2026-07-14-2.csv");

  Logbook::Write(book, {MakeEntry(10, 15, "a.igc"), MakeEntry(8, 9, "b.igc")});
  Logbook::AddToIndex(index, {"a.igc;1234"});

  /* named after the last flight; the index is gone, so all files are
     read again */
  const auto retired = Logbook::Retire(book, index);
  ok1(retired != nullptr && retired.GetBase() == old1);
  ok1(!File::Exists(book));
  ok1(!File::Exists(index));
  ok1(Logbook::Read(old1).size() == 2);

  /* an old log book of the same name is not replaced */
  Logbook::Write(book, {MakeEntry(10, 15, "a.igc")});
  const auto retired2 = Logbook::Retire(book, index);
  ok1(retired2 != nullptr && retired2.GetBase() == old2);

  /* without a log book there is nothing to rename */
  ok1(Logbook::Retire(book, index) == nullptr);

  File::Delete(old1);
  File::Delete(old2);
}

static void
WriteText(Path path, const char *text)
{
  FileOutputStream file(path);
  BufferedOutputStream writer(file);
  writer.Write(text);
  writer.Flush();
  file.Commit();
}

/**
 * A data directory as the program finds it: an IGC file of an older
 * version in "logs", a flight sorted into a subfolder of "igc", and an
 * NMEA log without a flight.
 */
static void
TestUpdateAndMove()
{
  const Path root("TestLogbookBuilder.tmp.dir");
  const auto igc = AllocatedPath::Build(root, "igc");
  const auto logs = AllocatedPath::Build(root, "logs");
  const auto sub = AllocatedPath::Build(igc, "2026");
  Directory::CreateRecursive(sub);
  Directory::CreateRecursive(logs);

  WriteShortFlight(AllocatedPath::Build(logs, "old.igc"));
  WriteShortFlight(AllocatedPath::Build(sub, "sorted.igc"));
  WriteText(AllocatedPath::Build(logs, "ground.nmea"),
            "$GPRMC,100000,A,5000.000,N,01000.000,E,0.0,0.0,140726,,*00\r\n");

  const auto book = AllocatedPath::Build(logs, "logbook.csv");
  const auto index = AllocatedPath::Build(logs, "logbook-files.txt");

  std::vector<AllocatedPath> folders;
  folders.push_back(AllocatedPath{Path{igc}});
  folders.push_back(AllocatedPath{Path{logs}});

  const auto files = Logbook::FindNewFiles(index, folders);
  /* the subfolder is searched as well */
  ok1(files.size() == 3);

  Logbook::MoveSettings move;
  move.igc_folder = AllocatedPath{Path{igc}};
  move.no_flight = true;

  NullOperationEnvironment env;
  const auto result = Logbook::Update(book, index, files, {}, env, move);
  ok1(result.files_read == 3);
  /* the two files record the same flight */
  ok1(result.flights == 1);
  ok1(result.moved_igc == 1);
  ok1(result.moved_no_flight == 1);
  ok1(File::Exists(AllocatedPath::Build(igc, "old.igc")));
  ok1(File::Exists(AllocatedPath::Build(AllocatedPath::Build(logs, "no-flight"),
                                        "ground.nmea")));
  ok1(Logbook::Read(book).size() == 1);

  /* nothing new: the moved files are known by name and size, and the
     folder "no-flight" is not searched */
  ok1(Logbook::FindNewFiles(index, folders).empty());

  File::Delete(AllocatedPath::Build(igc, "old.igc"));
  File::Delete(AllocatedPath::Build(sub, "sorted.igc"));
  File::Delete(AllocatedPath::Build(AllocatedPath::Build(logs, "no-flight"),
                                    "ground.nmea"));
  File::Delete(book);
  File::Delete(index);
  Directory::Remove(AllocatedPath::Build(logs, "no-flight"));
  Directory::Remove(sub);
  Directory::Remove(igc);
  Directory::Remove(logs);
  Directory::Remove(root);
}

int
main()
{
  plan_tests(57);

  TestIsComplete();
  TestRank();
  TestMergeEntry();
  TestJoin();
  TestIgcHeader();
  TestRetire();
  TestLandingAtRest();
  TestUpdateAndMove();

  return exit_status();
}
