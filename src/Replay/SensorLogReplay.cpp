// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "SensorLogReplay.hpp"
#include "NMEA/Info.hpp"
#include "Atmosphere/Pressure.hpp"
#include "system/Path.hpp"

#include <cmath>
#include <cstring>

/*
 * The LRSX block format: each record starts with one little endian
 * 32 bit word holding the record id (bits 0..7), the record length
 * in 32 bit units including this word (bits 8..15) and a CRC-16
 * over those two bytes (bits 16..31, polynomial 0x1021, seed
 * 0xfff1).  id = length = 255 announces an extended record with a
 * separate 32 bit id and length.
 */

namespace {

constexpr unsigned ID_BASIC_SENSOR_DATA = 20;
constexpr unsigned ID_GNSS_DATA = 30;
constexpr unsigned ID_D_GNSS_DATA = 31;
constexpr unsigned ID_EXTENDED = 255;

#pragma pack(push, 1)

/** record id 20, 13 words at 100 Hz */
struct SensorRecord {
  float acceleration[3];
  float rotation[3];
  float magnetic[3];
  float pitot_pressure;      // Pa
  float static_pressure;     // Pa
  float temperature;
  float supply_voltage;      // V
};

static_assert(sizeof(SensorRecord) == 13 * 4);

/** record id 30, 13 words per GNSS update; id 31 adds the D-GNSS
    relative position and heading */
struct GnssRecord {
  double latitude, longitude;   // degrees
  float msl_altitude;           // m
  float velocity[3];            // NED, m/s
  float speed_accuracy;         // m/s
  uint8_t year, month, day, hour, minute, second;
  uint8_t sat_count, sat_fix_type;
  int32_t nanoseconds;
  int16_t geo_separation_dm;
  uint16_t pdop;
};

static_assert(sizeof(GnssRecord) == 13 * 4);

struct DGnssTail {
  float relpos_ned[3];          // m
  float rel_heading;            // degrees
};

static_assert(sizeof(DGnssTail) == 4 * 4);

#pragma pack(pop)

/**
 * The header checksum as the sensor firmware computes it: one
 * CRC-16 table step (polynomial 0x1021) with seed 0xfff1 over the
 * low half of the header word - due to a byte cast in the firmware
 * only the id byte takes part.
 */
[[gnu::const]]
static uint16_t
HeaderCrc(uint16_t info) noexcept
{
  const auto table_entry = [](uint8_t index) {
    uint16_t r = uint16_t(index) << 8;
    for (unsigned i = 0; i < 8; ++i)
      r = (r & 0x8000) ? uint16_t((r << 1) ^ 0x1021) : uint16_t(r << 1);
    return r;
  };

  const uint8_t carry = uint8_t((0xfff1 >> 8) ^ info);
  return uint16_t(uint16_t(0xfff1 << 8) ^ table_entry(carry));
}

} // anonymous namespace

SensorLogReplay::SensorLogReplay(Path path)
  :file(path), reader(file)
{
  date.Clear();

  try {
    file_size = file.GetSize();
  } catch (...) {
  }
}

SensorLogReplay::~SensorLogReplay() = default;

bool
SensorLogReplay::SkipBytes(std::size_t n) noexcept
{
  try {
    std::byte scratch[256];
    while (n > 0) {
      const std::size_t chunk = std::min(n, sizeof(scratch));
      reader.ReadFull({scratch, chunk});
      consumed += chunk;
      n -= chunk;
    }
  } catch (...) {
    return false;
  }

  return true;
}

inline bool
SensorLogReplay::ReadFix(NMEAInfo &data) noexcept
{
  while (true) {
    uint32_t header;
    try {
      reader.ReadFullT(header);
    } catch (...) {
      return false;
    }
    consumed += sizeof(header);

    const unsigned id = header & 0xff;
    const unsigned length = (header >> 8) & 0xff;

    if (id == ID_EXTENDED && length == ID_EXTENDED) {
      /* an extended record: separate id and length words follow */
      uint32_t ext_id, ext_length;
      try {
        reader.ReadFullT(ext_id);
        reader.ReadFullT(ext_length);
      } catch (...) {
        return false;
      }
      consumed += 2 * sizeof(uint32_t);

      if (ext_length < 3 || !SkipBytes(std::size_t(ext_length - 3) * 4))
        return false;
      continue;
    }

    if (length < 1)
      /* corrupt stream: stop the replay */
      return false;

    /* the header checksum is computed but not enforced - firmware
       versions differ in how they feed the CRC, and the length
       checks below are protection enough */
    (void)HeaderCrc(uint16_t(header & 0xffff));

    const std::size_t payload = std::size_t(length - 1) * 4;

    if (id == ID_BASIC_SENSOR_DATA && payload >= sizeof(SensorRecord)) {
      SensorRecord r;
      try {
        reader.ReadFullT(r);
      } catch (...) {
        return false;
      }
      consumed += sizeof(r);

      std::memcpy(sensor.acceleration, r.acceleration,
                  sizeof(sensor.acceleration));
      sensor.pitot_pressure = r.pitot_pressure;
      sensor.static_pressure = r.static_pressure;
      sensor.voltage = r.supply_voltage;
      sensor.available = true;

      if (payload > sizeof(r) && !SkipBytes(payload - sizeof(r)))
        return false;
      continue;
    }

    if ((id == ID_GNSS_DATA && payload >= sizeof(GnssRecord)) ||
        (id == ID_D_GNSS_DATA &&
         payload >= sizeof(GnssRecord) + sizeof(DGnssTail))) {
      GnssRecord r;
      DGnssTail tail{};
      bool have_heading = false;
      try {
        reader.ReadFullT(r);
        consumed += sizeof(r);
        if (id == ID_D_GNSS_DATA) {
          reader.ReadFullT(tail);
          consumed += sizeof(tail);
          have_heading = true;
        }
      } catch (...) {
        return false;
      }

      const std::size_t known = sizeof(r) +
        (have_heading ? sizeof(tail) : std::size_t(0));
      if (payload > known && !SkipBytes(payload - known))
        return false;

      if ((r.sat_fix_type & 1) == 0)
        /* no fix: keep reading */
        continue;

      date = BrokenDate(2000 + r.year, r.month, r.day);
      if (!date.IsPlausible())
        continue;

      const BrokenTime time(r.hour, r.minute, r.second);
      if (!time.IsPlausible())
        continue;

      data.clock = TimeStamp{time.DurationSinceMidnight() +
                             FloatDuration{r.nanoseconds * 1e-9}};
      data.alive.Update(data.clock);
      data.ProvideTime(data.clock);
      data.ProvideDate(date);

      data.location = GeoPoint(Angle::Degrees(r.longitude),
                               Angle::Degrees(r.latitude));
      data.location_available.Update(data.clock);

      data.gps_altitude = r.msl_altitude;
      data.gps_altitude_available.Update(data.clock);

      data.gps.satellites_used = r.sat_count;
      data.gps.satellites_used_available.Update(data.clock);
      data.gps.real = false;
      data.gps.replay = true;

      const double vn = r.velocity[0], ve = r.velocity[1];
      data.ground_speed = std::hypot(vn, ve);
      data.ground_speed_available.Update(data.clock);
      data.track = Angle::FromXY(vn, ve).AsBearing();
      data.track_available.Update(data.clock);

      /* the GNSS vertical speed; XCSoar prefers the pressure based
         values it derives itself */
      data.ProvideNoncompVario(-r.velocity[2]);

      if (have_heading) {
        /* the D-GNSS heading is stored in radians */
        data.attitude.heading = Angle::Radians(tail.rel_heading).AsBearing();
        data.attitude.heading_available.Update(data.clock);
      }

      if (sensor.available) {
        data.ProvideStaticPressure(
            AtmosphericPressure::Pascal(sensor.static_pressure));
        data.ProvideDynamicPressure(
            AtmosphericPressure::Pascal(std::max(sensor.pitot_pressure,
                                                 0.f)));

        data.voltage = sensor.voltage;
        data.voltage_available.Update(data.clock);

        const double g = std::sqrt(sensor.acceleration[0] * sensor.acceleration[0] +
                                   sensor.acceleration[1] * sensor.acceleration[1] +
                                   sensor.acceleration[2] * sensor.acceleration[2])
          / 9.80665;
        data.acceleration.ProvideGLoad(g);
      }

      return true;
    }

    /* an unknown or unexpected record: skip its payload */
    if (!SkipBytes(payload))
      return false;
  }
}

bool
SensorLogReplay::Update(NMEAInfo &data)
{
  return ReadFix(data);
}
