// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "DeviceConfig.hpp"
#include "Map.hpp"
#include "util/Macros.hpp"
#include "Interface.hpp"
#include "Device/Config.hpp"

#ifdef ANDROID
#include "Android/BluetoothHelper.hpp"
#include "java/Global.hxx"
#endif

#include <stdio.h>

using std::string_view_literals::operator""sv;

const StaticString<128> empty128("");
const StaticString<64>  empty64("");
const StaticString<32>  empty32("");
const StaticString<32>  Disabled("Disabled");
const StaticString<64>  Disabled64("Disabled");
const StaticString<128> Disabled128("Disabled");

// constexpr 
static DeviceConfig StandardPort = {
  DeviceConfig::PortType::DISABLED,
0,  /* unsigned baud_rate; */
0,  /* unsigned bulk_baud_rate; */
Disabled64,  /* StaticString<64> path; */
Disabled128,  /* StaticString<128> port_name; */
empty32,  /* StaticString<32> bluetooth_mac; */
0,  /* unsigned ioio_uart_id; */
2,  /* unsigned i2c_bus; */
0,  /* unsigned i2c_addr; */
DeviceConfig::PressureUse::STATIC_ONLY, // NONE, // press_use; */
0.00,  /* double sensor_offset; */
0.00,  /* double sensor_factor; */
DeviceConfig::EngineType::NONE, /* engine_type; */
Disabled,  // StaticString<32> driver_name;
false,    /* use_second_device; */
Disabled, /*StaticString<32> driver2_name; */
empty64, /*StaticString<64> ip_address; */
// 4353, /*unsigned tcp_port; */
4352, /*unsigned tcp_port; */
false, /*bool enabled; */
false, /*bool k6bt; */

#ifndef NDEBUG
 false,  /* bool dump_port;*/
#endif
 true,  /* bool sync_to_device;*/
 true,  /* bool sync_from_device;*/
};
#if 0
};
#endif

static const char *const port_type_strings[] = {
  "disabled",
  "serial",
  "rfcomm",
  "rfcomm_server",
  "ioio_uart",
  "droidsoar_v2",
  "nunchuck",
  "i2c_baro",
  "ioio_voltage",
  "auto",
  "internal",
  "tcp_client",
  "tcp_listener",
  "udp_listener",
  "pty",
  "ble_sensor",
  "ble_hm10",
  "glider_link",
//  "android_usb_serial",
  "usb_serial",
  NULL
};

[[maybe_unused]]
static const char *
MakeDeviceSettingName(char *buffer, const char *prefix, unsigned n,
                      const char *suffix)
{
#if 1
  sprintf(buffer, "%s%u%s", prefix, n + 1,  suffix);
#else
  strcpy(buffer, prefix);

  if (n > 0)
    sprintf(buffer + strlen(buffer), "%u", n + 1);

  strcat(buffer, suffix);
#endif
  return buffer;
}

static bool
StringToPortType(std::string_view value, DeviceConfig::PortType &type)
{
  if (value == "android_usb_serial")
    value = "usb_serial";  // avoid the old name
  for (auto i = port_type_strings; *i != NULL; ++i) {
    // if (StringIsEqual(value, *i)) {
    if (value ==  *i) {
      type = (DeviceConfig::PortType)std::distance(port_type_strings, i);
      return true;
    }
  }

  return false;
}

static bool
ReadPortType(ProfileMap &map, unsigned n, DeviceConfig::PortType &type)
{
  char buffer[64];

  snprintf(buffer, sizeof(buffer), "Port%u%s", n + 1, "Type");

  [[maybe_unused]] int _type = 0;
  auto _default = port_type_strings[(int)StandardPort.port_type];
  std::string value = map.Get(buffer, _default);
  if (n== 0 && value == _default) {
    char buffer2[64];
    snprintf(buffer2, sizeof(buffer2), "Port%s", "Type");
    value = map.Get(buffer2, _default);
    map.Remove(buffer2);
    map.Set(buffer, value.c_str());
  }
  return StringToPortType(value, type);
}

#define GET_PORT_VALUE(_name, _index, _value, _default) \
  snprintf(buffer, sizeof(buffer), "Port%u%s", _index + 1, _name); \
  if (!map.Get(buffer, _value)) { \
    if (_index == 0) { \
      char buffer2[64]; \
      snprintf(buffer2, sizeof(buffer2), "Port%s", _name); \
      if (!map.Get(buffer2, _value)) \
         _value = _default; \
      map.Set(buffer, _value); \
    } \
  } \
  if (_index == 0) { \
    snprintf(buffer, sizeof(buffer), "Port%s", _name); \
    map.Remove(buffer); \
  } \
  if (_value == _default) { \
    map.Remove(buffer); \
  }
//--------------------------------------------------------

#define GET_PORT_ENUM(_name, _index, _value, _default) \
  snprintf(buffer, sizeof(buffer), "Port%u%s", _index + 1, _name); \
  if (!map.GetEnum(buffer, _value)) { \
    if (_index == 0) { \
      char buffer2[64]; \
      snprintf(buffer2, sizeof(buffer2), "Port%s",_name); \
      if (!map.GetEnum(buffer2, _value)) \
         _value = _default; \
      map.SetEnum(buffer, _value); \
    } \
  } \
  if (_index == 0) { \
    snprintf(buffer, sizeof(buffer), "Port%s", _name); \
    map.Remove(buffer); \
  } \
  if (_value == _default) { \
    map.Remove(buffer); \
  }
//--------------------------------------------------------
#define SAVE_PORT_VALUE(_name, _index, _value, _default) \
  snprintf(buffer, sizeof(buffer), "Port%u%s", _index + 1, _name); \
  if (_value != _default) { \
    map.Set(buffer, _value); \
  } else {  \
    map.Remove(buffer); \
  }
//--------------------------------------------------------

#define SAVE_PORT_ENUM(_name, _index, _value, _default) \
  snprintf(buffer, sizeof(buffer), "Port%u%s", _index + 1, _name); \
  if (_value != _default) { \
    map.SetEnum(buffer, _value); \
  } else {  \
    map.Remove(buffer); \
  }
//--------------------------------------------------------

static bool
LoadPath(ProfileMap &map, DeviceConfig &config, unsigned n)
{
  char buffer[64];
  GET_PORT_VALUE("Path", n, config.path, StandardPort.path);
#if 0
  snprintf(buffer, sizeof(buffer), "Port%u%s", n + 1, "Path");
  bool retvalue = map.Get(buffer, config.path);
  if (n == 0 && !retvalue) {
    snprintf(buffer, sizeof(buffer), "Port%s", "Path");
    retvalue = map.Get(buffer, config.path);
    map.Remove(buffer);
  }
//  if (!retvalue) {
//    config.path = StandardPort.path;
#endif

#ifdef _WIN32
  // the usual windows port names has no colon at the end
  if (!config.path.empty()) {
    if ((config.path.back() == char(':')) &&
      /* In Windows the value itself should be only have the short */
      config.path.StartsWith("COM")) {
      /* old-style raw names has a trailing colon (for backwards
       compatibility with older XCSoar versions) */
      config.path.Truncate(config.path.length() - 1);
      //  write back this (short) value to the profile:
      map.Set(buffer, config.path);
    }
    else if (config.path.StartsWith("\\\\.\\COM")) {
      /* since 7.30 the raw names in the XCSoar config has the UNC style
       (compatibility with this XCSoar versions config) */
      config.path = config.path + 4;
      // write back this (short) value to the profile:
      map.Set(buffer, config.path);  // write back to the profile
    }
  }
#endif
  return config.path != StandardPort.path;
}


void
Profile::GetDeviceConfig(ProfileMap &map, unsigned n,
  DeviceConfig &config)
{
  char buffer[64];

  config = StandardPort;

  bool have_port_type = ReadPortType(map, n, config.port_type);

  GET_PORT_VALUE("Driver", n, config.driver_name, StandardPort.driver_name);

#ifndef NEW_PORT_SETTING
  // TODO: 2025-05-26: remove this code after 1-2 years
  if (config.driver_name.empty() || 
    config.driver_name == StandardPort.driver_name) {
    // 2nd try with old "DriverA" - "DriverF" name
    strcpy(buffer, "DeviceA");
    buffer[strlen(buffer) - 1] += n;  // DeviceA - DeviceF
#ifdef OPENVARIO
    if (!map.Get(buffer, config.driver_name))
      if (n == 0)
        config.driver_name = "OpenVario";
#else
    map.Get(buffer, config.driver_name);
#endif
    map.Remove(buffer);
    if (map.Exists(buffer))
      printf("!!!!");
    SAVE_PORT_VALUE("Driver", n, config.driver_name, StandardPort.driver_name);
  }
#endif

  config.path.clear();
  if ((!have_port_type ||
    config.port_type == DeviceConfig::PortType::USB_SERIAL ||
#ifdef _WIN32
    config.port_type == DeviceConfig::PortType::BLE_HM10 ||
    config.port_type == DeviceConfig::PortType::BLE_SENSOR ||
#endif
    config.port_type == DeviceConfig::PortType::RFCOMM ||
    config.port_type == DeviceConfig::PortType::SERIAL) &&
    !LoadPath(map, config, n))
    config.port_type = DeviceConfig::PortType::SERIAL;

  GET_PORT_VALUE("BluetoothMAC", n, config.bluetooth_mac, StandardPort.bluetooth_mac);
  GET_PORT_VALUE("PortName", n, config.port_name, StandardPort.port_name);
  GET_PORT_VALUE("IOIOUartID", n, config.ioio_uart_id, StandardPort.ioio_uart_id);
  GET_PORT_VALUE("IPAddress", n, config.ip_address, StandardPort.ip_address);
  GET_PORT_VALUE("TCPPort", n, config.tcp_port, StandardPort.tcp_port);
  GET_PORT_VALUE("BaudRate", n, config.baud_rate, StandardPort.baud_rate);
  GET_PORT_VALUE("BulkBaudRate", n, config.bulk_baud_rate, StandardPort.bulk_baud_rate);
  GET_PORT_VALUE("Enabled", n, config.enabled, StandardPort.enabled);
  GET_PORT_VALUE("SyncFromDevice", n, config.sync_from_device, StandardPort.sync_from_device);
  GET_PORT_VALUE("SyncToDevice", n, config.sync_to_device, StandardPort.sync_to_device);
  GET_PORT_VALUE("K6Bt", n, config.k6bt, StandardPort.k6bt);
  GET_PORT_VALUE("I2C_Bus", n, config.i2c_bus, StandardPort.i2c_bus);
  GET_PORT_VALUE("I2C_Addr", n, config.i2c_addr, StandardPort.i2c_addr);
  GET_PORT_VALUE("SensorOffset", n, config.sensor_offset, StandardPort.sensor_offset);
  GET_PORT_VALUE("SensorFactor", n, config.sensor_factor, StandardPort.sensor_factor);
  GET_PORT_VALUE("UseSecondDevice", n, config.use_second_device, StandardPort.use_second_device);
  GET_PORT_VALUE("SecondDevice", n, config.driver2_name, StandardPort.driver2_name);

  GET_PORT_ENUM("PressureUse", n, config.press_use, StandardPort.press_use);
  GET_PORT_ENUM("EngineType", n, config.engine_type, StandardPort.engine_type);

#ifndef NDEBUG
  GET_PORT_VALUE("DumpPort", n, config.dump_port, StandardPort.dump_port);
#endif
  
  map.Remove("PortIgnoreChecksum");  // if available, not used anymore
}

static const char *
PortTypeToString(DeviceConfig::PortType type)
{
  const unsigned i = (unsigned)type;
  return i < ARRAY_SIZE(port_type_strings)
    ? port_type_strings[i]
    : NULL;
}

static void
WritePortType(ProfileMap &map, unsigned n, DeviceConfig::PortType type)
{
  const char *value = PortTypeToString(type);
  if (value == NULL)
    return;

  char buffer[64];
  snprintf(buffer, sizeof(buffer), "Port%u%s", n + 1, "Type");
//  MakeDeviceSettingName(buffer, "Port", n, "Type");
  map.Set(buffer, value);
}

// MakeDeviceSettingName(buffer, "Port", n, name);

void
Profile::SetDeviceConfig(ProfileMap &map,
                         unsigned n, const DeviceConfig &config)
{
  char buffer[64];

  WritePortType(map, n, config.port_type);

  SAVE_PORT_VALUE("Driver", n, config.driver_name, StandardPort.driver_name);

  SAVE_PORT_VALUE("Path", n, config.path, StandardPort.path);
  SAVE_PORT_VALUE("PortName", n, config.port_name, StandardPort.port_name);
  SAVE_PORT_VALUE("BluetoothMAC", n, config.bluetooth_mac, StandardPort.bluetooth_mac);
  SAVE_PORT_VALUE("IOIOUartID", n, config.ioio_uart_id, StandardPort.ioio_uart_id);
  SAVE_PORT_VALUE("IPAddress", n, config.ip_address, StandardPort.ip_address);
  SAVE_PORT_VALUE("TCPPort", n, config.tcp_port, StandardPort.tcp_port);
  SAVE_PORT_VALUE("BaudRate", n, config.baud_rate, StandardPort.baud_rate);
  SAVE_PORT_VALUE("BulkBaudRate", n, config.bulk_baud_rate, StandardPort.bulk_baud_rate);
  SAVE_PORT_VALUE("Enabled", n, config.enabled, StandardPort.enabled);
  SAVE_PORT_VALUE("SyncFromDevice", n, config.sync_from_device, StandardPort.sync_from_device);
  SAVE_PORT_VALUE("SyncToDevice", n, config.sync_to_device, StandardPort.sync_to_device);
  SAVE_PORT_VALUE("K6Bt", n, config.k6bt, StandardPort.k6bt);
  SAVE_PORT_VALUE("I2C_Bus", n, config.i2c_bus, StandardPort.i2c_bus);
  SAVE_PORT_VALUE("I2C_Addr", n, config.i2c_addr, StandardPort.i2c_addr);
  SAVE_PORT_VALUE("SensorOffset", n, config.sensor_offset, StandardPort.sensor_offset);
  SAVE_PORT_VALUE("SensorFactor", n, config.sensor_factor, StandardPort.sensor_factor);
  SAVE_PORT_VALUE("UseSecondDevice", n, config.use_second_device, StandardPort.use_second_device);
  SAVE_PORT_VALUE("SecondDevice", n, config.driver2_name, StandardPort.driver2_name);

  SAVE_PORT_ENUM("PressureUse", n, config.press_use, StandardPort.press_use);
  SAVE_PORT_ENUM("EngineType", n, config.engine_type, StandardPort.engine_type);
}
