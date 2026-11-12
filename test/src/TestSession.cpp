// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Session/SessionState.hpp"
#include "system/Path.hpp"

#include "TestUtil.hpp"

#include <stdio.h>

static bool
WriteRaw(Path path, const char *text) noexcept
{
  FILE *file = fopen(path.c_str(), "w");
  if (file == nullptr)
    return false;

  fputs(text, file);
  fclose(file);
  return true;
}

static void
TestRoundTrip()
{
  SessionState state;
  state.flying = true;
  state.have_ballast = true;
  state.ballast_litres = 120;
  state.have_crew = true;
  state.crew_mass = 95;
  state.have_bugs = true;
  state.bugs = 0.98;
  state.have_qnh = true;
  state.qnh_hpa = 1015;
  state.have_max_temp = true;
  state.max_temp_k = 300;
  state.have_atc_reference = true;
  state.atc_reference = GeoPoint(Angle::Degrees(8.5),
                                 Angle::Degrees(47.4));
  state.have_atc_declination = true;
  state.atc_declination_deg = -2;
  state.have_goto = true;
  state.goto_id = 42;
  state.have_goto_location = true;
  state.goto_location = GeoPoint(Angle::Degrees(8.51),
                                 Angle::Degrees(47.49));
  state.have_goto_elevation = true;
  state.goto_elevation = 450;
  state.goto_name = "(goto)";
  state.checklist_file = "checklists/xcsoar-checklist.txt";
  state.checklist_page = 1;
  state.checklist[0].present = true;
  state.checklist[0].sum = SessionHash("page");
  state.checklist[0].ticks = "1010";

  const Path path("output/TestSession.json");
  SaveSession(state, path);

  SessionState loaded;
  LoadSession(loaded, path);

  ok1(loaded.flying);
  ok1(equals(loaded.ballast_litres, 120));
  ok1(equals(loaded.crew_mass, 95));
  ok1(equals(loaded.bugs, 0.98));
  ok1(loaded.have_qnh);
  ok1(equals(loaded.qnh_hpa, 1015));
  ok1(loaded.have_max_temp);
  ok1(equals(loaded.max_temp_k, 300));
  ok1(loaded.have_atc_reference);
  ok1(equals(loaded.atc_reference.longitude.Degrees(), 8.5));
  ok1(equals(loaded.atc_reference.latitude.Degrees(), 47.4));
  ok1(equals(loaded.atc_declination_deg, -2));
  ok1(loaded.ShouldRestoreGoto());
  ok1(loaded.goto_id == 42);
  ok1(loaded.goto_name == "(goto)");
  ok1(equals(loaded.goto_elevation, 450));
  ok1(loaded.checklist_page == 1);
  ok1(loaded.checklist_file == "checklists/xcsoar-checklist.txt");

  std::string ticks;
  ok1(loaded.ChecklistTicks(0, SessionHash("page"), ticks));
  ok1(ticks == "1010");
  ok1(!loaded.modified);
  ok1(!loaded.seen_flying);
}

static void
TestFlyingClearIgnoresPolar()
{
  ok1(WriteRaw(Path("output/TestSessionFlying.txt"),
               "Flying=\"0\"\n"
               "BallastLitres=\"120\"\n"
               "CrewMass=\"95\"\n"
               "Bugs=\"0.98\"\n"));

  SessionState loaded;
  LoadLegacySession(loaded, Path("output/TestSessionFlying.txt"));
  ok1(!loaded.flying);
  ok1(loaded.have_ballast);
  ok1(equals(loaded.ballast_litres, 120));
}

static void
TestBadGeoPoint()
{
  ok1(WriteRaw(Path("output/TestSessionGeo.txt"),
               "AtcReference=\"999 10\"\n"));

  SessionState loaded;
  LoadLegacySession(loaded, Path("output/TestSessionGeo.txt"));
  ok1(!loaded.have_atc_reference);
  ok1(!loaded.atc_reference.IsValid());
}

static void
TestChecklistMismatch()
{
  SessionState state;
  state.checklist[0].present = true;
  state.checklist[0].sum = SessionHash("one");
  state.checklist[0].ticks = "11";

  std::string ticks;
  ok1(!state.ChecklistTicks(0, SessionHash("two"), ticks));
  ok1(state.ChecklistTicks(0, SessionHash("one"), ticks));
  ok1(ticks == "11");
  ok1(SessionHash("a") == SessionHash("a"));
  ok1(SessionHash("a") != SessionHash("b"));
}

static void
TestTakeoffGoto()
{
  SessionState state;
  state.have_goto = true;
  state.have_goto_location = true;
  state.goto_location = GeoPoint(Angle::Degrees(1), Angle::Degrees(2));
  state.goto_name = "(takeoff)";
  ok1(!state.ShouldRestoreGoto());

  state.goto_name = "(goto)";
  ok1(state.ShouldRestoreGoto());
}

static void
TestFlyingLatch()
{
  SessionState state;
  state.flying = true;
  state.have_ballast = true;
  state.ballast_litres = 40;

  ok1(!state.NoteFlying(false, false, 0, 90, 1));
  ok1(state.flying);
  ok1(equals(state.ballast_litres, 40));

  ok1(state.NoteFlying(true, false, 10, 80, 0.9));
  ok1(state.seen_flying);
  ok1(state.flying);
  ok1(equals(state.ballast_litres, 10));
  ok1(equals(state.crew_mass, 80));
  ok1(equals(state.bugs, 0.9));

  ok1(state.NoteFlying(false, false, 10, 80, 0.9));
  ok1(!state.flying);
  ok1(!state.NoteFlying(false, false, 0, 90, 1));
}

/**
 * A flight restored from the file whose landing this process never saw,
 * because it was restarted on the ground: the flying computer reports
 * "on the ground" and that ends the flight, while a restart in the air,
 * which is not on the ground yet, keeps it.
 */
static void
TestRestoredFlightLands()
{
  SessionState state;
  state.flying = true;

  ok1(!state.NoteFlying(false, false, 0, 90, 1));
  ok1(state.flying);

  ok1(state.NoteFlying(false, true, 0, 90, 1));
  ok1(!state.flying);
  ok1(state.modified);

  ok1(!state.NoteFlying(false, true, 0, 90, 1));
}

static void
TestDeclinedRestore()
{
  SessionState state;
  state.restore_declined = true;

  ok1(!state.NoteFlying(true, false, 10, 80, 0.9));
  ok1(!state.flying);
  ok1(!state.seen_flying);

  ok1(!state.NoteFlying(false, false, 10, 80, 0.9));
  ok1(!state.restore_declined);

  ok1(state.NoteFlying(true, false, 10, 80, 0.9));
  ok1(state.flying);
}

static void
TestNonFinite()
{
  ok1(WriteRaw(Path("output/TestSessionNonFinite.txt"),
               "BallastLitres=\"inf\"\n"
               "CrewMass=\"nan\"\n"
               "AtcDeclination=\"nan\"\n"
               "GotoElevation=\"inf\"\n"
               "GotoId=\"7\"\n"));

  SessionState loaded;
  LoadLegacySession(loaded, Path("output/TestSessionNonFinite.txt"));
  ok1(!loaded.have_ballast);
  ok1(!loaded.have_crew);
  ok1(!loaded.have_atc_declination);
  ok1(!loaded.have_goto_elevation);
  ok1(loaded.have_goto);
  ok1(loaded.goto_id == 7);
}


/**
 * A session written before the move to JSON must survive an update:
 * read the old format, write JSON, and get the same values back.
 */
static void
TestLegacyMigration()
{
  ok1(WriteRaw(Path("output/TestSessionLegacy.txt"),
               "Flying=\"1\"\n"
               "BallastLitres=\"80\"\n"
               "CrewMass=\"90\"\n"
               "Bugs=\"0.95\"\n"
               "QNH=\"1009\"\n"
               "AtcReference=\"8.5 47.4\"\n"
               "GotoId=\"12\"\n"
               "GotoName=\"Hahnweide\"\n"
               "ChecklistFile=\"checklists/x.txt\"\n"
               "ChecklistPage=\"2\"\n"
               "Checklist.3.Sum=\"4711\"\n"
               "Checklist.3.Ticks=\"101\"\n"));

  SessionState legacy;
  LoadLegacySession(legacy, Path("output/TestSessionLegacy.txt"));
  SaveSession(legacy, Path("output/TestSessionMigrated.json"));

  SessionState loaded;
  LoadSession(loaded, Path("output/TestSessionMigrated.json"));
  ok1(loaded.flying);
  ok1(loaded.have_ballast && equals(loaded.ballast_litres, 80));
  ok1(loaded.have_crew && equals(loaded.crew_mass, 90));
  ok1(loaded.have_bugs && equals(loaded.bugs, 0.95));
  ok1(loaded.have_qnh && equals(loaded.qnh_hpa, 1009));
  ok1(loaded.have_atc_reference);
  ok1(equals(loaded.atc_reference.latitude.Degrees(), 47.4));
  ok1(loaded.goto_id == 12);
  ok1(loaded.goto_name == "Hahnweide");
  ok1(loaded.checklist_file == "checklists/x.txt");
  ok1(loaded.checklist_page == 2);

  std::string ticks;
  ok1(loaded.ChecklistTicks(3, 4711, ticks));
  ok1(ticks == "101");
}

/**
 * Values of the wrong type or out of range are dropped one by one;
 * the rest of the file still counts.
 */
static void
TestJsonInvalidValues()
{
  ok1(WriteRaw(Path("output/TestSessionBad.json"),
               "{\"flying\":\"yes\","
               "\"polar\":{\"ballast_litres\":-5,\"crew_mass\":\"90\","
               "\"bugs\":0.9},"
               "\"atmosphere\":{\"qnh_hpa\":5000,\"max_temperature_k\":1e999},"
               "\"atc\":{\"reference\":{\"longitude\":999,\"latitude\":10}},"
               "\"goto\":{\"id\":-3,\"name\":\"(goto)\"},"
               "\"checklist\":{\"file\":\"c.txt\",\"pages\":["
               "{\"index\":99,\"sum\":1,\"ticks\":\"1\"},"
               "{\"index\":0,\"sum\":1,\"ticks\":\"1x\"},"
               "{\"index\":1,\"sum\":2,\"ticks\":\"01\"}]}}"));

  SessionState loaded;
  LoadSession(loaded, Path("output/TestSessionBad.json"));
  ok1(!loaded.flying);
  ok1(!loaded.have_ballast);
  ok1(!loaded.have_crew);
  ok1(loaded.have_bugs && equals(loaded.bugs, 0.9));
  ok1(!loaded.have_qnh);
  ok1(!loaded.have_max_temp);
  ok1(!loaded.have_atc_reference);
  ok1(loaded.goto_id == 0);
  ok1(loaded.goto_name == "(goto)");
  ok1(!loaded.checklist[0].present);
  ok1(loaded.checklist[1].present);
}

/** A file that is not a JSON object at all is an error, not defaults. */
static void
TestJsonNotAnObject()
{
  ok1(WriteRaw(Path("output/TestSessionArray.json"), "[1,2,3]"));

  SessionState loaded;
  bool thrown = false;
  try {
    LoadSession(loaded, Path("output/TestSessionArray.json"));
  } catch (...) {
    thrown = true;
  }
  ok1(thrown);
}

/** The takeoff block survives a round trip, pilot and aircraft too. */
static void
TestTakeoffRoundTrip()
{
  SessionState state;
  state.flying = true;
  state.have_takeoff = true;
  state.takeoff_utc = BrokenDateTime(2026, 7, 14, 10, 23, 45);
  state.takeoff_location = GeoPoint(Angle::Degrees(9.43),
                                    Angle::Degrees(48.69));
  state.have_takeoff_altitude = true;
  state.takeoff_altitude = 512;
  state.pilot_name = "Uwe";
  state.aircraft_registration = "D-1234";
  state.competition_id = "UA";
  state.aircraft_type = "LS 8";

  SaveSession(state, Path("output/TestSessionTakeoff.json"));

  SessionState loaded;
  LoadSession(loaded, Path("output/TestSessionTakeoff.json"));
  ok1(loaded.have_takeoff);
  ok1(loaded.takeoff_utc == BrokenDateTime(2026, 7, 14, 10, 23, 45));
  ok1(loaded.takeoff_location.IsValid());
  ok1(equals(loaded.takeoff_location.latitude.Degrees(), 48.69));
  ok1(loaded.have_takeoff_altitude && equals(loaded.takeoff_altitude, 512));
  ok1(loaded.pilot_name == "Uwe");
  ok1(loaded.aircraft_registration == "D-1234");
  ok1(loaded.competition_id == "UA");
  ok1(loaded.aircraft_type == "LS 8");
  /* only a takeoff seen by this process may be recorded */
  ok1(!loaded.takeoff_pending);
}

/** A takeoff without a valid time is not a takeoff. */
static void
TestTakeoffBadTime()
{
  ok1(WriteRaw(Path("output/TestSessionTakeoffBad.json"),
               "{\"flying\":true,\"flight\":{\"takeoff\":"
               "{\"time\":\"2026-13-40T25:00:00Z\",\"altitude\":500},"
               "\"pilot\":\"Uwe\"}}"));

  SessionState loaded;
  LoadSession(loaded, Path("output/TestSessionTakeoffBad.json"));
  ok1(loaded.flying);
  ok1(!loaded.have_takeoff);
  ok1(!loaded.have_takeoff_altitude);
}

/**
 * A takeoff after "not flying" starts a new flight and forgets the
 * old takeoff; a restart in the air keeps the recorded one and must
 * not replace it with the moment of the restart.
 */
static void
TestTakeoffPending()
{
  SessionState state;
  state.have_takeoff = true;
  state.takeoff_utc = BrokenDateTime(2026, 7, 13, 9, 0, 0);
  state.aircraft_registration = "D-OLD";

  ok1(state.NoteFlying(true, false, 0, 90, 1));
  ok1(state.takeoff_pending);
  ok1(!state.have_takeoff);
  ok1(state.aircraft_registration.empty());

  SessionState restarted;
  restarted.flying = true;
  restarted.have_takeoff = true;
  restarted.takeoff_utc = BrokenDateTime(2026, 7, 14, 10, 23, 45);
  restarted.NoteFlying(true, false, 0, 90, 1);
  ok1(!restarted.takeoff_pending);
  ok1(restarted.have_takeoff);
}

int
main()
{
  plan_tests(22 + 4 + 3 + 5 + 2 + 12 + 7 + 7 + 14 + 12 + 2 + 10 + 4 + 6 + 6);

  TestRoundTrip();
  TestFlyingClearIgnoresPolar();
  TestBadGeoPoint();
  TestChecklistMismatch();
  TestTakeoffGoto();
  TestFlyingLatch();
  TestRestoredFlightLands();
  TestDeclinedRestore();
  TestNonFinite();
  TestLegacyMigration();
  TestJsonInvalidValues();
  TestJsonNotAnObject();
  TestTakeoffRoundTrip();
  TestTakeoffBadTime();
  TestTakeoffPending();

  return exit_status();
}
