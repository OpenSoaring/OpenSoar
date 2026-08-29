// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "PortsConfig.hpp"
#include "LocalPath.hpp"
#include "LogFile.hpp"
#include "Profile/DeviceConfig.hpp"
#include "io/FileOutputStream.hxx"
#include "io/FileReader.hxx"
#include "json/Parse.hxx"
#include "json/Serialize.hxx"
#include "system/FileUtil.hpp"
#include "system/Path.hpp"

#include "Profile/File.hpp"
#include "Profile/Map.hpp"

#include <boost/json.hpp>

#include "util/StringAPI.hxx"

#include <string>

static constexpr int DEVICE_PORTS_VERSION = 1;

AllocatedPath
DevicePorts::GetPath() noexcept
{
  const Path dir = GetSystemConfigPath();
  if (dir == nullptr)
    return nullptr;

  return AllocatedPath::Build(dir, Path{DevicePorts::FILE_NAME});
}

static const char *
GetString(const boost::json::object &o, const char *key,
          const char *default_value="") noexcept
{
  if (const auto *value = o.if_contains(key); value != nullptr)
    if (const auto *s = value->if_string(); s != nullptr)
      return s->c_str();

  return default_value;
}

static bool
GetBool(const boost::json::object &o, const char *key,
        bool default_value) noexcept
{
  if (const auto *value = o.if_contains(key); value != nullptr)
    if (const auto *b = value->if_bool(); b != nullptr)
      return *b;

  return default_value;
}

static unsigned
GetUnsigned(const boost::json::object &o, const char *key,
            unsigned default_value=0) noexcept
{
  if (const auto *value = o.if_contains(key); value != nullptr) {
    if (const auto *i = value->if_int64(); i != nullptr && *i >= 0)
      return unsigned(*i);
    if (const auto *u = value->if_uint64(); u != nullptr)
      return unsigned(*u);
  }

  return default_value;
}

static double
GetDouble(const boost::json::object &o, const char *key,
          double default_value=0) noexcept
{
  if (const auto *value = o.if_contains(key); value != nullptr) {
    if (const auto *d = value->if_double(); d != nullptr)
      return *d;
    if (const auto *i = value->if_int64(); i != nullptr)
      return double(*i);
    if (const auto *u = value->if_uint64(); u != nullptr)
      return double(*u);
  }

  return default_value;
}

template<typename T>
static T
GetEnum(const boost::json::object &o, const char *key,
        T default_value, T max_value) noexcept
{
  const unsigned value = GetUnsigned(o, key, unsigned(default_value));
  return value < unsigned(max_value) ? T(value) : default_value;
}

/**
 * A device configuration as it comes out of DeviceConfig::Clear():
 * the one definition of "default" for both writing and reading.
 */
static const DeviceConfig &
GetDefaults() noexcept
{
  static const DeviceConfig defaults = []{
    DeviceConfig config;
    config.Clear();
    return config;
  }();

  return defaults;
}

static boost::json::object
ToJson(const DeviceConfig &config, unsigned index) noexcept
{
  boost::json::object o;

  const DeviceConfig &d = GetDefaults();

  /* index and type say which slot this is and what it is; everything
     else is written only when it differs from the default, so the
     file stays short and shows at a glance what was configured.  A
     setting put back to its default therefore disappears again - the
     whole file is rewritten on every save, so nothing stale is left
     behind */
  o["index"] = index;

  if (const char *type = Profile::PortTypeToString(config.port_type);
      type != nullptr)
    o["type"] = type;

  const auto set_string = [&](const char *key, const auto &value,
                              const auto &default_value) {
    if (value != default_value)
      o[key] = value.c_str();
  };

  const auto set = [&](const char *key, auto value, auto default_value) {
    if (value != default_value)
      o[key] = value;
  };

  set_string("driver", config.driver_name, d.driver_name);
  set("enabled", config.enabled, d.enabled);
  set("use_second_driver", config.use_second_device, d.use_second_device);
  set_string("second_driver", config.driver2_name, d.driver2_name);
  set_string("path", config.path, d.path);
  set_string("port_name", config.port_name, d.port_name);
  set_string("bluetooth_mac", config.bluetooth_mac, d.bluetooth_mac);
  set("baud_rate", config.baud_rate, d.baud_rate);
  set("bulk_baud_rate", config.bulk_baud_rate, d.bulk_baud_rate);
  set_string("ip_address", config.ip_address, d.ip_address);
  set("tcp_port", config.tcp_port, d.tcp_port);
  set("ioio_uart_id", config.ioio_uart_id, d.ioio_uart_id);
  set("i2c_bus", config.i2c_bus, d.i2c_bus);
  set("i2c_addr", config.i2c_addr, d.i2c_addr);
  set("pressure_use", unsigned(config.press_use), unsigned(d.press_use));
  set("sensor_offset", config.sensor_offset, d.sensor_offset);
  set("sensor_factor", config.sensor_factor, d.sensor_factor);
  set("engine_type", unsigned(config.engine_type), unsigned(d.engine_type));
  set("polar_sync", unsigned(config.polar_sync), unsigned(d.polar_sync));
  set("k6bt", config.k6bt, d.k6bt);
#ifndef NDEBUG
  set("dump_port", config.dump_port, d.dump_port);
#endif
  set("sync_from_device", config.sync_from_device, d.sync_from_device);
  set("sync_to_device", config.sync_to_device, d.sync_to_device);
  set("send_position", config.send_position, d.send_position);

  return o;
}

/**
 * Is this slot in its factory state?  Such a slot is left out of the
 * file entirely.
 */
static bool
IsDefault(const boost::json::object &o) noexcept
{
  /* nothing beyond "index" and a "type" of "disabled" */
  return o.size() <= 2 &&
    StringIsEqual(GetString(o, "type", "disabled"), "disabled");
}

static void
FromJson(const boost::json::object &o, DeviceConfig &config) noexcept
{
  /* Clear() sets every field to its default; from here on only the
     keys the file actually contains change anything - the same
     defaults ToJson() leaves out */
  config.Clear();

  if (!Profile::StringToPortType(GetString(o, "type", "disabled"),
                                 config.port_type))
    config.port_type = DeviceConfig::PortType::DISABLED;

  config.enabled = GetBool(o, "enabled", config.enabled);
  config.driver_name = GetString(o, "driver", config.driver_name.c_str());
  config.use_second_device = GetBool(o, "use_second_driver",
                                     config.use_second_device);
  config.driver2_name = GetString(o, "second_driver",
                                  config.driver2_name.c_str());
  config.path = GetString(o, "path", config.path.c_str());
  config.port_name = GetString(o, "port_name", config.port_name.c_str());
  config.bluetooth_mac = GetString(o, "bluetooth_mac",
                                   config.bluetooth_mac.c_str());
  config.baud_rate = GetUnsigned(o, "baud_rate", config.baud_rate);
  config.bulk_baud_rate = GetUnsigned(o, "bulk_baud_rate",
                                      config.bulk_baud_rate);
  config.ip_address = GetString(o, "ip_address", config.ip_address.c_str());
  config.tcp_port = GetUnsigned(o, "tcp_port", config.tcp_port);
  config.ioio_uart_id = GetUnsigned(o, "ioio_uart_id", config.ioio_uart_id);
  config.i2c_bus = GetUnsigned(o, "i2c_bus", config.i2c_bus);
  config.i2c_addr = GetUnsigned(o, "i2c_addr", config.i2c_addr);
  config.press_use = GetEnum(o, "pressure_use", config.press_use,
                             DeviceConfig::PressureUse::PITOT);
  config.sensor_offset = GetDouble(o, "sensor_offset", config.sensor_offset);
  config.sensor_factor = GetDouble(o, "sensor_factor", config.sensor_factor);
  config.engine_type = GetEnum(o, "engine_type", config.engine_type,
                               DeviceConfig::EngineType::MAX);
  config.polar_sync = GetEnum(o, "polar_sync", config.polar_sync,
                              DeviceConfig::PolarSync::COUNT);
  config.k6bt = GetBool(o, "k6bt", config.k6bt);
#ifndef NDEBUG
  config.dump_port = GetBool(o, "dump_port", config.dump_port);
#endif
  config.sync_from_device = GetBool(o, "sync_from_device",
                                    config.sync_from_device);
  config.sync_to_device = GetBool(o, "sync_to_device", config.sync_to_device);
  config.send_position = GetBool(o, "send_position", config.send_position);
}

/**
 * Translate a key of an old OpenSoar device_ports.xcd to the spelling
 * Profile::GetDeviceConfig() understands: that file numbered every
 * port ("Port1Type" for the first one, which upstream spells
 * "PortType"), and it kept the driver in "Port<N>Driver" instead of
 * "DeviceA", "DeviceB", ...
 */
static std::string
TranslateLegacyKey(const std::string &key) noexcept
{
  if (!key.starts_with("Port"))
    return key;

  std::size_t i = 4;
  unsigned number = 0;
  while (i < key.length() && key[i] >= '0' && key[i] <= '9') {
    number = number * 10 + unsigned(key[i] - '0');
    ++i;
  }

  if (number < 1 || number > NUMDEV)
    return key;

  const std::string suffix = key.substr(i);

  if (suffix == "Driver") {
    std::string driver_key = "DeviceA";
    driver_key.back() += char(number - 1);
    return driver_key;
  }

  /* the first port has no number upstream; the others are already
     numbered the same way */
  return number == 1
    ? "Port" + suffix
    : key;
}

bool
DevicePorts::LoadLegacy(Path path,
                        std::array<DeviceConfig, NUMDEV> &devices) noexcept
{
  ProfileMap raw;

  try {
    Profile::LoadFile(raw, path);
  } catch (...) {
    LogError(std::current_exception(),
             "Failed to read the old device port file");
    return false;
  }

  if (raw.begin() == raw.end())
    return false;

  ProfileMap map;
  for (const auto &[key, value] : raw)
    map.Set(TranslateLegacyKey(key), value.c_str());

  for (unsigned i = 0; i < NUMDEV; ++i) {
    devices[i].Clear();
    Profile::GetDeviceConfig(map, i, devices[i]);
  }

  return true;
}

bool
DevicePorts::Load(std::array<DeviceConfig, NUMDEV> &devices) noexcept
{
  const auto path = GetPath();
  if (path == nullptr || !File::Exists(path))
    return false;

  boost::json::value root;

  try {
    FileReader file{path};
    root = Json::Parse(file);
  } catch (...) {
    LogError(std::current_exception(), "Failed to read the device ports");
    return false;
  }

  const auto *object = root.if_object();
  if (object == nullptr)
    return false;

  const auto *ports = object->if_contains("ports");
  if (ports == nullptr)
    return false;

  const auto *array = ports->if_array();
  if (array == nullptr)
    return false;

  /* slots that the file does not mention stay empty rather than
     keeping whatever the profile had */
  for (auto &i : devices)
    i.Clear();

  for (const auto &i : *array) {
    const auto *port = i.if_object();
    if (port == nullptr)
      continue;

    const unsigned index = GetUnsigned(*port, "index", NUMDEV);
    if (index >= NUMDEV)
      continue;

    FromJson(*port, devices[index]);
  }

  return true;
}

bool
DevicePorts::Save(const std::array<DeviceConfig, NUMDEV> &devices) noexcept
{
  if (MakeSystemConfigPath() == nullptr) {
    LogString("Cannot create the directory for the device ports");
    return false;
  }

  const auto path = GetPath();
  if (path == nullptr)
    return false;

  boost::json::object root;
  root["version"] = DEVICE_PORTS_VERSION;

  boost::json::array ports;
  for (unsigned i = 0; i < devices.size(); ++i) {
    auto port = ToJson(devices[i], i);
    if (IsDefault(port))
      /* an untouched slot is not worth a line */
      continue;

    ports.emplace_back(std::move(port));
  }

  root["ports"] = std::move(ports);

  try {
    FileOutputStream file{path};
    Json::Serialize(file, root);
    file.Commit();
  } catch (...) {
    LogError(std::current_exception(), "Failed to write the device ports");
    return false;
  }

  return true;
}
