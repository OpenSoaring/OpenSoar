// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

/**
* see Documentation https://github.com/larus-breeze/doc_larus,
* for the driver you need https://github.com/larus-breeze/doc_larus/blob/master/documentation/Larus_NMEA_Protocol.md
* and an emulator is here https://github.com/larus-breeze/sw_tools
*/


#include "Device/Driver/Larus.hpp"
#include "Device/Driver.hpp"
#include "Device/Port/Port.hpp"
#include "Device/Util/NMEAWriter.hpp"
#include "NMEA/Checksum.hpp"
#include "NMEA/Info.hpp"
#include "NMEA/Derived.hpp"
#include "NMEA/InputLine.hpp"
#include "Units/System.hpp"
#include "Operation/Operation.hpp"
#include "LogFile.hpp"
#include "util/StaticString.hxx"

#include <span>

class LarusDevice : public AbstractDevice {
  Port &port;

public:
  LarusDevice(Port &_port) : port(_port) {}

  /* virtual methods from class Device */
  bool ParseNMEA(const char *line, NMEAInfo &info) override;
  bool PutMacCready(double mc, OperationEnvironment &env) override;
  bool PutBugs(double bugs, OperationEnvironment &env) override;
  bool PutBallast(double fraction, double overload,
                  OperationEnvironment &env) override;
  bool PutQNH(const AtmosphericPressure &pres,
              OperationEnvironment &env) override;

private:
  static bool PLARA(NMEAInputLine &line, NMEAInfo &info);
  static bool PLARB(NMEAInputLine &line, NMEAInfo &info);
  static bool PLARD(NMEAInputLine &line, NMEAInfo &info);
  static bool PLARV(NMEAInputLine &line, NMEAInfo &info);
  static bool PLARS(NMEAInputLine &line, NMEAInfo &info);
  static bool PLARW(NMEAInputLine &line, NMEAInfo &info);
  static bool HCHDT(NMEAInputLine &line, NMEAInfo &info);

  bool SendCmd(const char *cmd, double value, OperationEnvironment &env);
};

//-----------------------------------------------------------------------------
/**
 * Parses non-negative floating-point angle value in degrees.
 */
static bool
ReadBearing(NMEAInputLine &line, Angle &value_r)
{
    double value;
    if (!line.ReadChecked(value))
        return false;

    if (value < 0 || value > 360)
        return false;

    value_r = Angle::Degrees(value).AsBearing();
    return true;
}

bool
LarusDevice::ParseNMEA(const char *_line, NMEAInfo &info)
{
  if (!VerifyNMEAChecksum(_line))
    return false;

  NMEAInputLine line(_line);
  const auto type = line.ReadView();
  if (type.starts_with("$PLAR")) {
    switch (type[5]) {
    case 'A':
      return PLARA(line, info);
    case 'B':
      return PLARB(line, info);
    case 'D':
      return PLARD(line, info);
    case 'V':
      return PLARV(line, info);
    case 'W':
      return PLARW(line, info);
    case 'S':
      return PLARS(line, info);
    default:
      break;
    }
  }
  else if (type == "$HCHDT")
    return HCHDT(line, info);

  return false;
}

bool
LarusDevice::HCHDT(NMEAInputLine &line, NMEAInfo &info)
{
    /*
   * Heading sentence
     *
     *        1   2 3
     *        |   | |
     * $HCHDT,x.x,a*hh<CR><LF>
     * 
     * State of Heading
     *
     * Field Number:
     * 1)  Heading 
     * 2)  Type: (T)rue or (M)agnetic
     * 3)  Checksum
    */
  double value;
  if (line.ReadChecked(value)) {
    if (value >= 0 && value <= 360) {
      switch (line.ReadOneChar()) {
      case 'T':
          info.attitude.heading = Angle::Degrees(value);
          info.attitude.heading_available.Update(info.clock);
        return true;
      case 'M':
      default:
        return false;  // false means: an other (general) parser should look
      }
    }
  }
  return false;
}

bool
LarusDevice::PLARA(NMEAInputLine &line, NMEAInfo &info)
{
    /*
     * Attitude-Sentence
     *
     *        1   2   3   4
     *        |   |   |   |
     * $PLARA,x.x,x.x,x.x*hh<CR><LF>
     * 
     * This sentence gives information about the current attitude. The different fields
     * have the following meaning:
     * 
     * Field Number:
     * 1)  Roll angle (positive while turning right)
     * 2)  Pitch angle (positive when nose up)
     * 3)  Yaw angle (true heading)
     * 4)  Checksum
    */
  double value;
  if (line.ReadChecked(value)) {
    if (value >= -180 && value <= 180) {
      info.attitude.bank_angle = Angle::Degrees(value);
      info.attitude.bank_angle_available.Update(info.clock);
    }
  }
  if (line.ReadChecked(value)) {
    if (value >= -90 && value <= 90) {
      info.attitude.pitch_angle = Angle::Degrees(value);
      info.attitude.pitch_angle_available.Update(info.clock);
    }
  }
  if (line.ReadChecked(value)) {
    if (value >= 0 && value <= 360) {
      info.attitude.heading = Angle::Degrees(value);
      info.attitude.heading_available.Update(info.clock);
    }
  }
    return true;
}

bool
LarusDevice::PLARB(NMEAInputLine &line, NMEAInfo &info)
{
    /*
     * Battery voltage sentence
     *
     *        1     2
     *        |     |
     * $PLARB,xx.xx*hh<CR><LF>
     * 
     * Field Number:
     * 1)  battery voltage in Volt
     * 2)  Checksum
    */
  double value;
  if (line.ReadChecked(value)) {
    if (value >= 0 && value <= 25) {
      info.voltage = value;
      info.voltage_available.Update(info.clock);
    }
  }
  return true;
}

bool
LarusDevice::PLARD([[maybe_unused]] NMEAInputLine &line,
                   [[maybe_unused]] NMEAInfo &info)
{
   /*
   * Instant air density sentence
     *
     *        1      2 3
     *        |      | |
     * $PLARD,xxxx.x,a*hh<CR><LF>
     * 
     * This sentence gives information about the instant air density at the
     * current altitude. The different fields have the following meaning:
     *
     * Field Number:
     * 1)  Instant air density in g/m^3.
     * 2)  a = (M)easured or (E)stimated
     * 3)  Checksum
    */
#if 1 // aug: SimSys has an other opinion?
  // XCSoar/OpenSoar don't support that the air density is provided from the
  // outside.
#else
  double value;
  if (line.ReadChecked(value)) {
    switch (line.ReadOneChar()) {
    case 'M':
      break;
    case 'E':
      // TODO(Augut2111): is this correct???
      // the density behaves similar to static pressure, but isn't equal

      // info.static_pressure.HectoPascal(value);
      // info.static_pressure_available.Update(info.clock);
      break;
    default:
      break;
    }
  }
#endif
  return true;
}

bool
LarusDevice::PLARV(NMEAInputLine &line, NMEAInfo &info)
{
  /*
    * $PLARV,x.x,x.x,x,x.x*hh
    *
    * Vario Data: TEK vario, average vario, height (std pressure)
    *             and speed (tas)
    * 
    * Field Number:
    *  1) Total Energy Variometer (TEK vario)
    *  2) Average Climb Rate over one circle
    *  3) Pressure Height
    *  4) True Air Speed (TAS)
    *  5) Checksum
    */

  double value;
  // Parse total energy variometer
  if (line.ReadChecked(value)) {
    info.ProvideTotalEnergyVario(
        Units::ToSysUnit(value, Unit::METER_PER_SECOND));
  }

  // TODO(August2111): remove comments:
  // Parse average climb rate, Larus is doing this over one circle!
  // line.ReadChecked(value); // Skip average vario data, TODO(August2111):
  line.Skip();
  // create a new field for a Full Circle Average Climbrate(!),  make it
  // visible and set it here

  // Parse barometric altitude
  double altitude;  // used in ProvideTrueAirspeedWithAltitude too
  if (line.ReadChecked(altitude)) {
    altitude = Units::ToSysUnit(altitude, Unit::METER);
    info.ProvidePressureAltitude(altitude);
  }

  // Parse true airspeed
  if (line.ReadChecked(value))
    info.ProvideTrueAirspeedWithAltitude(
      Units::ToSysUnit(value, Unit::KILOMETER_PER_HOUR), altitude
    );

  return true;
}

bool
LarusDevice::PLARW(NMEAInputLine &line, NMEAInfo &info)
{
  /*
    * $PLARW,x.x,x.x,t,v*hh
    *
    * Field Number:
    *  1) wind angle
    *  2) wind speed
    *  3) t = (A)verage or (I)nstantaneous
    *  4) v = Status A=valid
    *  5) Checksum
    */
  SpeedVector wind;
  if (!ReadBearing(line, wind.bearing))
        return false;

  double windspeed;
  if (!line.ReadChecked(windspeed))
      return false;
  wind.norm = Units::ToSysUnit(windspeed, Unit::KILOMETER_PER_HOUR);

  switch (line.ReadOneChar()) {
  case 'A':
    info.ProvideExternalWind(wind);
    break;
  case 'I':
    info.ProvideExternalInstantaneousWind(wind);
    break;
  default:
    return false;
  }

  return true;
}

/*
$PLARS Settings parameters bidirectional

       1 2 3   4
       | | |   |
$PLARS,a,a,xxx*hh<CR><LF>

Examples:
$PLARS,L,MC,1.3*1E
$PLARS,L,BAL,1.25*6B
$PLARS,L,BUGS,15*3B
$PLARS,L,QNH,1013.2*74

$PLARS,H,MC,2.1*1B
$PLARS,H,BAL,1.00*68
$PLARS,H,BUGS,0*0B
$PLARS,H,QNH,1031.4*76

The $PLARS record is intended for exchanging setting values between Larus and a host system such as 
XCSoar. The record can be used in both directions: from host to Larus or from Larus to host.

These records should not be sent cyclically, but only when needed during initialization and when 
changes are made.

    Data source (L: Larus, H: Host)
    Settings parameter
        MC MacCready (0.0 - 9.9)
        BAL Ballast (1.00 - 1.60)
        BUGS Bugs in % (0 - 30)
        // Simsys: BAL Ballast (0.00 - 1.00)
        // Simsys: BUGS Bugs in % (0 - 50)
        QNH QNH in hPa
    Value (format depends on settings parameter, see examples)
    Checksum
*/
bool
LarusDevice::PLARS(NMEAInputLine &line, NMEAInfo &info)
{ // the line starts with the host indicator
  if (line.ReadOneChar() == 'L') {
    double value;
    auto field = line.ReadView();
    if (line.ReadChecked(value)) {
      if (field == "MC") {
        // - value is MacCReady in m/s [0.0 .. 9.9]
        return info.settings.ProvideMacCready(value, info.clock);
      } else if (field == "BUGS") {
        // - value is bugs in % [0-30], OpenSoar wants [0.5 .. 1.00]
        return info.settings.ProvideBugs((1.0 - value)/100.0, info.clock);
      } else if (field == "BAL") {
        // - value is ballast overload [1.00..1.60]
        return info.settings.ProvideBallastOverload(value, info.clock);
      } else if (field == "QNH") {
        // - value is pressure in hPa
        return info.settings.ProvideQNH(AtmosphericPressure::HectoPascal(value),
                                        info.clock);
      }
    }
  }
  return false;
}

bool
LarusDevice::PutBugs(double bugs, OperationEnvironment &env)
{
  // $PLARS,H,BUGS,0*0B
  // OpenSoar/XCSoar has values between [0.5 ..1.0]
  if ((bugs < 0.7) || (bugs > 1.0))
    return false;
  return SendCmd("BUGS,%u", uround((1.0-bugs) * 100), env);
}

bool
LarusDevice::PutMacCready(double mc, OperationEnvironment &env)
{
  // $PLARS,H,MC,2.1*1B
  return SendCmd("MC,%0.1f", mc, env);
}

bool
LarusDevice::PutBallast([[maybe_unused]] double fraction, double overload, 
                        OperationEnvironment &env)
{ 
  // $PLARS,H,BAL,1.00*68
  if ((overload < 1.0) || (overload > 1.60))
    return false;
  return SendCmd("BAL,%0.2f", overload, env);
}

bool
LarusDevice::PutQNH(const AtmosphericPressure &pres,
                          OperationEnvironment &env) 
{ 
  // $PLARS,H,QNH,1031.4*76
  return SendCmd("QNH,%0.1f", pres.GetHectoPascal(), env);
}

bool
LarusDevice::SendCmd(const char *cmd, double value, 
                              OperationEnvironment &env) {
  StaticString<80> buffer("PLARS,H,");
  buffer.AppendFormat(cmd, value);

  PortWriteNMEA(port, buffer.c_str(), env);
  return true;
}


static Device *
LarusCreateOnPort([[maybe_unused]] const DeviceConfig &config, Port &com_port)
{
  return new LarusDevice(com_port);
}

const struct DeviceRegister larus_driver = {
  "Larus",
  "Larus",
  DeviceRegister::SEND_SETTINGS,
  LarusCreateOnPort,
};
