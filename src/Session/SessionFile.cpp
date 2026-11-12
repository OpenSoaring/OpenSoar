// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "SessionState.hpp"

#include "LocalPath.hpp"
#include "LogFile.hpp"
#include "Math/Classify.hpp"
#include "Profile/File.hpp"
#include "Profile/Map.hpp"
#include "io/FileOutputStream.hxx"
#include "io/FileReader.hxx"
#include "json/Geo.hpp"
#include "json/Parse.hxx"
#include "json/Serialize.hxx"
#include "system/FileUtil.hpp"
#include "system/Path.hpp"

#include <boost/json.hpp>

#include <limits>
#include <stdexcept>
#include <stdio.h>
#include <utility>

/* JSON groups the values by topic, which the flat key=value format of
   a profile could only imitate with key prefixes, and the extension
   keeps the file out of the airspace file list, which claims *.txt. */
static constexpr char SESSION_FILE[] = "flight-session.json";

/* The name and format before the move to JSON.  Read once to take
   over an unfinished flight across an update, then removed. */
static constexpr char LEGACY_SESSION_FILE[] = "xcsoar-session.txt";

static constexpr unsigned SESSION_FILE_VERSION = 1;
static constexpr std::string_view TAKEOFF_GOTO{"(takeoff)"};

static constexpr double QNH_MIN_HPA = 100;
static constexpr double QNH_MAX_HPA = 1200;
static constexpr double TEMP_MIN_K = 200;
static constexpr double TEMP_MAX_K = 350;

static bool
InOpenRange(double value, double min, double max) noexcept
{
  return value > min && value < max;
}

static bool
SetPolar(SessionState &state, double ballast, double crew,
         double bugs) noexcept
{
  if (state.have_ballast && state.have_crew && state.have_bugs &&
      state.ballast_litres == ballast && state.crew_mass == crew &&
      state.bugs == bugs)
    return false;

  state.ballast_litres = ballast;
  state.crew_mass = crew;
  state.bugs = bugs;
  state.have_ballast = true;
  state.have_crew = true;
  state.have_bugs = true;
  state.modified = true;
  return true;
}

static SessionState session_state;
static bool session_ready = false;

static bool
ValidTicks(std::string_view ticks) noexcept
{
  for (const char c : ticks)
    if (c != '0' && c != '1')
      return false;

  return true;
}

static void
ChecklistKey(char *buffer, std::size_t size,
             unsigned page, const char *field) noexcept
{
  snprintf(buffer, size, "Checklist.%u.%s", page, field);
}

static void
ReadLegacyChecklist(const ProfileMap &map, SessionState &state) noexcept
{
  for (unsigned i = 0; i < SESSION_MAX_CHECKLIST_PAGES; ++i) {
    char sum_key[32];
    char ticks_key[32];
    ChecklistKey(sum_key, sizeof(sum_key), i, "Sum");
    ChecklistKey(ticks_key, sizeof(ticks_key), i, "Ticks");

    unsigned sum = 0;
    std::string ticks;
    if (!map.Get(sum_key, sum) || !map.Get(ticks_key, ticks) ||
        !ValidTicks(ticks))
      continue;

    auto &page = state.checklist[i];
    page.present = true;
    page.sum = sum;
    page.ticks = std::move(ticks);
  }
}

uint32_t
SessionHash(std::string_view text) noexcept
{
  uint32_t hash = 0x811c9dc5u;
  for (unsigned char c : text) {
    hash ^= c;
    hash *= 0x01000193u;
  }
  return hash;
}

bool
SessionState::ShouldRestoreGoto() const noexcept
{
  if (!have_goto || goto_name == TAKEOFF_GOTO)
    return false;

  return goto_id != 0 || have_goto_location;
}

bool
SessionState::ChecklistTicks(unsigned page, uint32_t sum,
                             std::string &out) const noexcept
{
  if (page >= SESSION_MAX_CHECKLIST_PAGES)
    return false;

  const auto &slot = checklist[page];
  if (!slot.present || slot.sum != sum)
    return false;

  out = slot.ticks;
  return true;
}

void
SessionState::ClearTakeoff() noexcept
{
  have_takeoff = false;
  takeoff_utc = BrokenDateTime::Invalid();
  takeoff_location.SetInvalid();
  have_takeoff_altitude = false;
  takeoff_altitude = 0;
  pilot_name.clear();
  aircraft_registration.clear();
  competition_id.clear();
  aircraft_type.clear();
}

bool
SessionState::NoteFlying(bool now_flying, bool on_ground, double ballast,
                         double crew, double _bugs) noexcept
{
  if (now_flying) {
    /* No wrote Flying=0 while still airborne.  Do not put it back. */
    if (restore_declined)
      return false;

    /* The file said "not flying": this is a takeoff, and what is
       recorded about an earlier flight no longer applies. */
    if (!flying) {
      ClearTakeoff();
      takeoff_pending = true;
    }

    const bool edge = !seen_flying || !flying;
    seen_flying = true;
    flying = true;
    const bool polar = SetPolar(*this, ballast, crew, _bugs);
    if (edge)
      modified = true;
    return edge || polar;
  }

  restore_declined = false;

  /* Not flying is no landing yet after a restart: the flying computer
     needs a few seconds of movement before it reports a restored flight
     as flying.  Only a flight this process has seen, or "on the
     ground", which needs ten seconds without movement, ends it.
     Without the latter a flight restored on the ground would stay
     open until the next takeoff and landing. */
  if (!flying || (!seen_flying && !on_ground))
    return false;

  flying = false;
  modified = true;
  return true;
}

void
LoadLegacySession(SessionState &state, Path path)
{
  ProfileMap map;
  /* shortcut: LoadFile also migrates AirspaceFile and WPFile keys,
     use a plain read if this file ever carries those names */
  Profile::LoadFile(map, path);

  SessionState loaded;

  unsigned flying = 0;
  if (map.Get("Flying", flying) && flying <= 1)
    loaded.flying = flying == 1;

  if (double litres; map.Get("BallastLitres", litres) &&
      IsFinite(litres) && litres >= 0) {
    loaded.ballast_litres = litres;
    loaded.have_ballast = true;
  }

  if (double mass; map.Get("CrewMass", mass) &&
      IsFinite(mass) && mass >= 0) {
    loaded.crew_mass = mass;
    loaded.have_crew = true;
  }

  if (double bugs; map.Get("Bugs", bugs) && bugs > 0 && bugs <= 1) {
    loaded.bugs = bugs;
    loaded.have_bugs = true;
  }

  if (double qnh; map.Get("QNH", qnh) &&
      InOpenRange(qnh, QNH_MIN_HPA, QNH_MAX_HPA)) {
    loaded.qnh_hpa = qnh;
    loaded.have_qnh = true;
  }

  if (double kelvin; map.Get("MaxTemp", kelvin) &&
      InOpenRange(kelvin, TEMP_MIN_K, TEMP_MAX_K)) {
    loaded.max_temp_k = kelvin;
    loaded.have_max_temp = true;
  }

  GeoPoint location = GeoPoint::Invalid();
  if (map.GetGeoPoint("AtcReference", location)) {
    loaded.atc_reference = location;
    loaded.have_atc_reference = true;
  }

  if (double degrees; map.Get("AtcDeclination", degrees) &&
      IsFinite(degrees)) {
    loaded.atc_declination_deg = degrees;
    loaded.have_atc_declination = true;
  }

  if (unsigned id; map.Get("GotoId", id)) {
    loaded.goto_id = id;
    loaded.have_goto = true;
  }

  location = GeoPoint::Invalid();
  if (map.GetGeoPoint("GotoLocation", location)) {
    loaded.goto_location = location;
    loaded.have_goto_location = true;
    loaded.have_goto = true;
  }

  if (double elevation; map.Get("GotoElevation", elevation) &&
      IsFinite(elevation)) {
    loaded.goto_elevation = elevation;
    loaded.have_goto_elevation = true;
    loaded.have_goto = true;
  }

  if (std::string name; map.Get("GotoName", name)) {
    loaded.goto_name = std::move(name);
    loaded.have_goto = true;
  }

  map.Get("ChecklistFile", loaded.checklist_file);
  if (unsigned page; map.Get("ChecklistPage", page))
    loaded.checklist_page = page;

  ReadLegacyChecklist(map, loaded);
  state = std::move(loaded);
}

/* JSON helpers.  A value of the wrong type or out of range counts as
   missing, so a damaged file degrades to the defaults field by field
   instead of losing the whole session. */

static const boost::json::object *
GetObject(const boost::json::object &parent, std::string_view key) noexcept
{
  const auto *v = parent.if_contains(key);
  return v != nullptr ? v->if_object() : nullptr;
}

static bool
GetNumber(const boost::json::object &parent, std::string_view key,
          double &value) noexcept
{
  const auto *v = parent.if_contains(key);
  if (v == nullptr || !v->is_number())
    return false;

  const double d = v->to_number<double>();
  if (!IsFinite(d))
    return false;

  value = d;
  return true;
}

static bool
GetUnsigned(const boost::json::object &parent, std::string_view key,
            unsigned &value) noexcept
{
  const auto *v = parent.if_contains(key);
  if (v == nullptr)
    return false;

  if (const auto *i = v->if_int64(); i != nullptr && *i >= 0 &&
      *i <= std::numeric_limits<unsigned>::max()) {
    value = static_cast<unsigned>(*i);
    return true;
  }

  if (const auto *u = v->if_uint64();
      u != nullptr && *u <= std::numeric_limits<unsigned>::max()) {
    value = static_cast<unsigned>(*u);
    return true;
  }

  return false;
}

static bool
GetString(const boost::json::object &parent, std::string_view key,
          std::string &value) noexcept
{
  const auto *v = parent.if_contains(key);
  if (v == nullptr || !v->is_string())
    return false;

  value = v->get_string();
  return true;
}

/** The counterpart of the GeoPoint conversion in json/Geo.hpp. */
static bool
GetGeoPoint(const boost::json::object &parent, std::string_view key,
            GeoPoint &value) noexcept
{
  const auto *o = GetObject(parent, key);
  if (o == nullptr)
    return false;

  double longitude, latitude;
  if (!GetNumber(*o, "longitude", longitude) ||
      !GetNumber(*o, "latitude", latitude))
    return false;

  const GeoPoint p{Angle::Degrees(longitude), Angle::Degrees(latitude)};
  if (!p.Check())
    return false;

  value = p;
  return true;
}

static void
ReadChecklist(const boost::json::object &checklist,
              SessionState &state) noexcept
{
  GetString(checklist, "file", state.checklist_file);
  if (unsigned page; GetUnsigned(checklist, "page", page))
    state.checklist_page = page;

  const auto *pages = checklist.if_contains("pages");
  if (pages == nullptr || !pages->is_array())
    return;

  for (const auto &item : pages->get_array()) {
    const auto *o = item.if_object();
    if (o == nullptr)
      continue;

    unsigned index, sum;
    std::string ticks;
    if (!GetUnsigned(*o, "index", index) ||
        index >= SESSION_MAX_CHECKLIST_PAGES ||
        !GetUnsigned(*o, "sum", sum) ||
        !GetString(*o, "ticks", ticks) || !ValidTicks(ticks))
      continue;

    auto &page = state.checklist[index];
    page.present = true;
    page.sum = sum;
    page.ticks = std::move(ticks);
  }
}

/** ISO 8601 in UTC, e.g. "2026-11-12T10:23:45Z". */
static std::string
FormatUTC(const BrokenDateTime &t) noexcept
{
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%04u-%02u-%02uT%02u:%02u:%02uZ",
           t.year, t.month, t.day, t.hour, t.minute, t.second);
  return buffer;
}

static bool
ParseUTC(const std::string &s, BrokenDateTime &t) noexcept
{
  unsigned year, month, day, hour, minute, second;
  char z;
  if (sscanf(s.c_str(), "%4u-%2u-%2uT%2u:%2u:%2u%c",
             &year, &month, &day, &hour, &minute, &second, &z) != 7 ||
      z != 'Z')
    return false;

  const BrokenDateTime parsed(year, month, day, hour, minute, second);
  if (!parsed.IsPlausible())
    return false;

  t = parsed;
  return true;
}

static void
ReadFlight(const boost::json::object &flight, SessionState &state) noexcept
{
  if (const auto *takeoff = GetObject(flight, "takeoff")) {
    std::string time;
    if (GetString(*takeoff, "time", time) &&
        ParseUTC(time, state.takeoff_utc)) {
      state.have_takeoff = true;
      GetGeoPoint(*takeoff, "location", state.takeoff_location);

      if (double altitude; GetNumber(*takeoff, "altitude", altitude)) {
        state.takeoff_altitude = altitude;
        state.have_takeoff_altitude = true;
      }
    }
  }

  GetString(flight, "pilot", state.pilot_name);

  if (const auto *aircraft = GetObject(flight, "aircraft")) {
    GetString(*aircraft, "registration", state.aircraft_registration);
    GetString(*aircraft, "competition_id", state.competition_id);
    GetString(*aircraft, "type", state.aircraft_type);
  }
}

void
LoadSession(SessionState &state, Path path)
{
  FileReader file{path};
  const auto root_value = Json::Parse(file);
  const auto *root = root_value.if_object();
  if (root == nullptr)
    throw std::runtime_error("Session file is not a JSON object");

  SessionState loaded;

  if (const auto *v = root->if_contains("flying");
      v != nullptr && v->is_bool())
    loaded.flying = v->get_bool();

  if (const auto *polar = GetObject(*root, "polar")) {
    if (double litres; GetNumber(*polar, "ballast_litres", litres) &&
        litres >= 0) {
      loaded.ballast_litres = litres;
      loaded.have_ballast = true;
    }

    if (double mass; GetNumber(*polar, "crew_mass", mass) && mass >= 0) {
      loaded.crew_mass = mass;
      loaded.have_crew = true;
    }

    if (double bugs; GetNumber(*polar, "bugs", bugs) &&
        bugs > 0 && bugs <= 1) {
      loaded.bugs = bugs;
      loaded.have_bugs = true;
    }
  }

  if (const auto *atmosphere = GetObject(*root, "atmosphere")) {
    if (double qnh; GetNumber(*atmosphere, "qnh_hpa", qnh) &&
        InOpenRange(qnh, QNH_MIN_HPA, QNH_MAX_HPA)) {
      loaded.qnh_hpa = qnh;
      loaded.have_qnh = true;
    }

    if (double kelvin; GetNumber(*atmosphere, "max_temperature_k", kelvin) &&
        InOpenRange(kelvin, TEMP_MIN_K, TEMP_MAX_K)) {
      loaded.max_temp_k = kelvin;
      loaded.have_max_temp = true;
    }
  }

  if (const auto *atc = GetObject(*root, "atc")) {
    if (GetGeoPoint(*atc, "reference", loaded.atc_reference))
      loaded.have_atc_reference = true;

    if (double degrees; GetNumber(*atc, "declination_deg", degrees)) {
      loaded.atc_declination_deg = degrees;
      loaded.have_atc_declination = true;
    }
  }

  if (const auto *go = GetObject(*root, "goto")) {
    if (unsigned id; GetUnsigned(*go, "id", id)) {
      loaded.goto_id = id;
      loaded.have_goto = true;
    }

    if (GetGeoPoint(*go, "location", loaded.goto_location)) {
      loaded.have_goto_location = true;
      loaded.have_goto = true;
    }

    if (double elevation; GetNumber(*go, "elevation", elevation)) {
      loaded.goto_elevation = elevation;
      loaded.have_goto_elevation = true;
      loaded.have_goto = true;
    }

    if (std::string name; GetString(*go, "name", name)) {
      loaded.goto_name = std::move(name);
      loaded.have_goto = true;
    }
  }

  if (const auto *checklist = GetObject(*root, "checklist"))
    ReadChecklist(*checklist, loaded);

  if (const auto *flight = GetObject(*root, "flight"))
    ReadFlight(*flight, loaded);

  state = std::move(loaded);
}

static boost::json::value
GeoPointValue(const GeoPoint &p) noexcept
{
  return boost::json::value_from(p);
}

void
SaveSession(const SessionState &state, Path path)
{
  boost::json::object root;
  root["version"] = SESSION_FILE_VERSION;
  root["flying"] = state.flying;

  boost::json::object polar;
  if (state.have_ballast)
    polar["ballast_litres"] = state.ballast_litres;
  if (state.have_crew)
    polar["crew_mass"] = state.crew_mass;
  if (state.have_bugs)
    polar["bugs"] = state.bugs;
  if (!polar.empty())
    root["polar"] = std::move(polar);

  boost::json::object atmosphere;
  if (state.have_qnh)
    atmosphere["qnh_hpa"] = state.qnh_hpa;
  if (state.have_max_temp)
    atmosphere["max_temperature_k"] = state.max_temp_k;
  if (!atmosphere.empty())
    root["atmosphere"] = std::move(atmosphere);

  boost::json::object atc;
  if (state.have_atc_reference)
    atc["reference"] = GeoPointValue(state.atc_reference);
  if (state.have_atc_declination)
    atc["declination_deg"] = state.atc_declination_deg;
  if (!atc.empty())
    root["atc"] = std::move(atc);

  if (state.have_goto && state.goto_name != TAKEOFF_GOTO) {
    boost::json::object go;
    if (state.goto_id != 0)
      go["id"] = state.goto_id;
    if (state.have_goto_location)
      go["location"] = GeoPointValue(state.goto_location);
    if (state.have_goto_elevation)
      go["elevation"] = state.goto_elevation;
    go["name"] = state.goto_name;
    root["goto"] = std::move(go);
  }

  if (!state.checklist_file.empty()) {
    boost::json::object checklist;
    checklist["file"] = state.checklist_file;
    checklist["page"] = state.checklist_page;

    boost::json::array pages;
    for (unsigned i = 0; i < SESSION_MAX_CHECKLIST_PAGES; ++i) {
      const auto &page = state.checklist[i];
      if (!page.present)
        continue;

      pages.push_back(boost::json::object{
        {"index", i},
        {"sum", page.sum},
        {"ticks", page.ticks},
      });
    }

    checklist["pages"] = std::move(pages);
    root["checklist"] = std::move(checklist);
  }

  if (state.have_takeoff) {
    boost::json::object takeoff;
    takeoff["time"] = FormatUTC(state.takeoff_utc);
    if (state.takeoff_location.IsValid())
      takeoff["location"] = GeoPointValue(state.takeoff_location);
    if (state.have_takeoff_altitude)
      takeoff["altitude"] = state.takeoff_altitude;

    boost::json::object aircraft;
    if (!state.aircraft_registration.empty())
      aircraft["registration"] = state.aircraft_registration;
    if (!state.competition_id.empty())
      aircraft["competition_id"] = state.competition_id;
    if (!state.aircraft_type.empty())
      aircraft["type"] = state.aircraft_type;

    boost::json::object flight;
    flight["takeoff"] = std::move(takeoff);
    if (!state.pilot_name.empty())
      flight["pilot"] = state.pilot_name;
    if (!aircraft.empty())
      flight["aircraft"] = std::move(aircraft);
    root["flight"] = std::move(flight);
  }

  /* FileOutputStream writes a temporary file and renames it on
     Commit(), so a power cut while writing leaves the previous
     session intact. */
  FileOutputStream file{path};
  Json::Serialize(file, boost::json::value{std::move(root)});
  file.Commit();
}

namespace Session {

SessionState &
State() noexcept
{
  return session_state;
}

/**
 * Take over a session written before the move to JSON, so that an
 * update installed between two flights - or during one - does not
 * lose the restore question.  The old file is removed only after the
 * new one has been written.
 */
static void
MigrateLegacy() noexcept
{
  const auto legacy_path = LocalPath(LEGACY_SESSION_FILE);
  if (!File::Exists(legacy_path))
    return;

  try {
    LoadLegacySession(session_state, legacy_path);
    SaveSession(session_state, LocalPath(SESSION_FILE));
    File::Delete(legacy_path);
    LogFmt("Session: migrated {} to {}", LEGACY_SESSION_FILE, SESSION_FILE);
  } catch (...) {
    LogError(std::current_exception(), "Failed to migrate session");
    session_state = {};
  }
}

void
Load() noexcept
{
  const auto path = LocalPath(SESSION_FILE);
  if (!File::Exists(path)) {
    MigrateLegacy();
    return;
  }

  try {
    LoadSession(session_state, path);
  } catch (...) {
    LogError(std::current_exception(), "Failed to load session");
    session_state = {};
  }

  /* A legacy file next to a JSON one is a leftover of an interrupted
     migration; the JSON file is the newer of the two. */
  const auto legacy_path = LocalPath(LEGACY_SESSION_FILE);
  if (File::Exists(legacy_path))
    File::Delete(legacy_path);
}

void
Flush() noexcept
{
  if (!session_state.modified)
    return;

  try {
    SaveSession(session_state, LocalPath(SESSION_FILE));
    session_state.modified = false;
  } catch (...) {
    LogError(std::current_exception(), "Failed to save session");
  }
}

bool
IsReady() noexcept
{
  return session_ready;
}

void
SetReady() noexcept
{
  session_ready = true;
}

void
DeclineRestore() noexcept
{
  if (!session_state.flying)
    return;

  session_state.flying = false;
  session_state.restore_declined = true;
  /* the declined flight is over as far as the session is concerned */
  session_state.ClearTakeoff();
  session_state.modified = true;
  Flush();
}

static void
NoteValue(bool &have, double &slot, double value) noexcept
{
  if (have && slot == value)
    return;

  slot = value;
  have = true;
  session_state.modified = true;
  Flush();
}

void
NotePolar(double ballast_litres, double crew_mass, double bugs) noexcept
{
  SetPolar(session_state, ballast_litres, crew_mass, bugs);
}

void
NoteQNH(double hpa) noexcept
{
  if (!InOpenRange(hpa, QNH_MIN_HPA, QNH_MAX_HPA))
    return;

  NoteValue(session_state.have_qnh, session_state.qnh_hpa, hpa);
}

void
NoteMaxTemp(double kelvin) noexcept
{
  if (!InOpenRange(kelvin, TEMP_MIN_K, TEMP_MAX_K))
    return;

  NoteValue(session_state.have_max_temp, session_state.max_temp_k, kelvin);
}

void
NoteAtcReference(const GeoPoint &reference) noexcept
{
  if (!reference.IsValid()) {
    ClearAtcReference();
    return;
  }

  auto &state = session_state;
  if (state.have_atc_reference && state.atc_reference == reference)
    return;

  state.atc_reference = reference;
  state.have_atc_reference = true;
  state.modified = true;
  Flush();
}

void
NoteTakeoff(const BrokenDateTime &utc, const GeoPoint &location,
            bool has_altitude, double altitude,
            std::string_view pilot_name,
            std::string_view registration,
            std::string_view competition_id,
            std::string_view aircraft_type) noexcept
{
  auto &state = session_state;
  if (!state.takeoff_pending || !state.flying || !utc.IsPlausible())
    return;

  state.takeoff_pending = false;
  state.have_takeoff = true;
  state.takeoff_utc = utc;
  state.takeoff_location = location;
  state.have_takeoff_altitude = has_altitude && IsFinite(altitude);
  state.takeoff_altitude = state.have_takeoff_altitude ? altitude : 0;
  state.pilot_name = pilot_name;
  state.aircraft_registration = registration;
  state.competition_id = competition_id;
  state.aircraft_type = aircraft_type;
  state.modified = true;
  Flush();
}

void
ClearAtcReference() noexcept
{
  auto &state = session_state;
  if (!state.have_atc_reference)
    return;

  state.atc_reference.SetInvalid();
  state.have_atc_reference = false;
  state.modified = true;
  Flush();
}

void
NoteAtcDeclination(double degrees) noexcept
{
  NoteValue(session_state.have_atc_declination,
            session_state.atc_declination_deg, degrees);
}

void
NoteGoto(unsigned id, const GeoPoint &location, bool has_elevation,
         double elevation, std::string_view name) noexcept
{
  if (name == TAKEOFF_GOTO)
    return;

  auto &state = session_state;
  state.have_goto = true;
  state.goto_id = id;
  state.have_goto_location = location.IsValid();
  if (state.have_goto_location)
    state.goto_location = location;
  state.have_goto_elevation = has_elevation;
  if (has_elevation)
    state.goto_elevation = elevation;
  state.goto_name = name;
  state.modified = true;
  Flush();
}

void
ClearGoto() noexcept
{
  auto &state = session_state;
  if (!state.have_goto && state.goto_id == 0 && !state.have_goto_location &&
      state.goto_name.empty())
    return;

  state.have_goto = false;
  state.goto_id = 0;
  state.have_goto_location = false;
  state.goto_location.SetInvalid();
  state.have_goto_elevation = false;
  state.goto_elevation = 0;
  state.goto_name.clear();
  state.modified = true;
  Flush();
}

void
NoteChecklist(std::string_view path, unsigned page,
              std::span<const SessionChecklistNote> pages) noexcept
{
  auto &state = session_state;
  std::array<SessionChecklistPage, SESSION_MAX_CHECKLIST_PAGES> next{};
  unsigned n = pages.size();
  if (n > SESSION_MAX_CHECKLIST_PAGES)
    n = SESSION_MAX_CHECKLIST_PAGES;

  for (unsigned i = 0; i < n; ++i) {
    next[i].present = true;
    next[i].sum = pages[i].sum;
    next[i].ticks = pages[i].ticks;
  }

  if (state.checklist_file == path && state.checklist_page == page &&
      state.checklist == next)
    return;

  state.checklist_file = path;
  state.checklist_page = page;
  state.checklist = std::move(next);
  state.modified = true;
  Flush();
}

void
ClearChecklist() noexcept
{
  auto &state = session_state;
  const std::array<SessionChecklistPage,
                   SESSION_MAX_CHECKLIST_PAGES> empty{};
  if (state.checklist_file.empty() && state.checklist_page == 0 &&
      state.checklist == empty)
    return;

  state.checklist_file.clear();
  state.checklist_page = 0;
  state.checklist = {};
  state.modified = true;
  Flush();
}

}
