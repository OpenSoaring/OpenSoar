// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

/*
 * The device ports live in a file of their own now.  The bug this
 * guards against is an old one from OpenSoar: the settings were read
 * from the new file but written to the old one, so every driver stayed
 * at its previous value and nobody noticed until a device did not
 * answer.  Hence: write, read back, compare - field by field, drivers
 * included.
 */

#include "Device/PortsConfig.hpp"
#include "Device/Config.hpp"
#include "LocalPath.hpp"
#include "TestUtil.hpp"
#include "io/FileOutputStream.hxx"
#include "io/FileReader.hxx"
#include "system/FileUtil.hpp"
#include "system/Path.hpp"
#include "util/SpanCast.hxx"

#include <array>
#include <span>
#include <string>
#include <string_view>

#include <stdlib.h>

static void
WriteTextFile(Path path, const char *content)
{
  FileOutputStream out(path, FileOutputStream::Mode::CREATE);
  out.Write(AsBytes(std::string_view(content)));
  out.Commit();
}

static std::string
ReadTextFile(Path path)
{
  FileReader file{path};
  char buffer[4096];
  const auto nbytes = file.Read(std::as_writable_bytes(std::span{buffer}));
  return std::string{buffer, nbytes};
}

static DeviceConfig
MakeConfig(DeviceConfig::PortType type, const char *path,
           unsigned baud_rate, const char *driver) noexcept
{
  DeviceConfig config;
  config.Clear();
  config.port_type = type;
  config.path = path;
  config.baud_rate = baud_rate;
  config.driver_name = driver;
  config.enabled = true;
  return config;
}

static void
TestSaveLoadRoundTrip()
{
  std::array<DeviceConfig, NUMDEV> devices;
  for (auto &i : devices)
    i.Clear();

  devices[0] = MakeConfig(DeviceConfig::PortType::SERIAL, "COM3:",
                          38400, "LX");
  devices[1] = MakeConfig(DeviceConfig::PortType::TCP_CLIENT, "",
                          0, "Condor");
  devices[1].ip_address = "192.168.1.7";
  devices[1].tcp_port = 4353;
  devices[1].use_second_device = true;
  devices[1].driver2_name = "FLARM";
  devices[1].sync_from_device = false;
  devices[1].k6bt = true;

  ok1(DevicePorts::Save(devices));

  std::array<DeviceConfig, NUMDEV> loaded;
  for (auto &i : loaded)
    i.Clear();

  ok1(DevicePorts::Load(loaded));

  /* the driver is the field the old bug got wrong */
  ok1(loaded[0].driver_name == devices[0].driver_name);
  ok1(loaded[1].driver_name == devices[1].driver_name);
  ok1(loaded[1].driver2_name == devices[1].driver2_name);
  ok1(loaded[1].use_second_device);

  ok1(loaded[0].port_type == DeviceConfig::PortType::SERIAL);
  ok1(loaded[0].path == devices[0].path);
  ok1(loaded[0].baud_rate == 38400);
  ok1(loaded[0].enabled);

  ok1(loaded[1].port_type == DeviceConfig::PortType::TCP_CLIENT);
  ok1(loaded[1].ip_address == devices[1].ip_address);
  ok1(loaded[1].tcp_port == 4353);
  ok1(!loaded[1].sync_from_device);
  ok1(loaded[1].k6bt);

  /* slots the file does not use come back empty */
  ok1(loaded[2].port_type == DeviceConfig::PortType::DISABLED);
}

/**
 * A second save must reach the same file: the old bug wrote the new
 * value somewhere else, so a reload produced the previous driver.
 */
static void
TestSaveOverwrites()
{
  std::array<DeviceConfig, NUMDEV> devices;
  for (auto &i : devices)
    i.Clear();

  devices[0] = MakeConfig(DeviceConfig::PortType::SERIAL, "COM1:",
                          4800, "Volkslogger");
  ok1(DevicePorts::Save(devices));

  devices[0].driver_name = "Vega";
  devices[0].baud_rate = 57600;
  ok1(DevicePorts::Save(devices));

  std::array<DeviceConfig, NUMDEV> loaded;
  ok1(DevicePorts::Load(loaded));
  ok1(loaded[0].driver_name == StaticString<32>("Vega"));
  ok1(loaded[0].baud_rate == 57600);
}

/**
 * The key=value file of the old OpenSoar: every port numbered from 1,
 * and the driver in "Port<N>Driver".
 */
static void
TestLegacyConversion()
{
  const auto path = AllocatedPath::Build(GetSystemConfigPath(),
                                         Path{"legacy_ports.xcd"});

  WriteTextFile(path,
                "Port1Type=\"serial\"\n"
                "Port1Path=\"/dev/ttyUSB0\"\n"
                "Port1BaudRate=\"19200\"\n"
                "Port1Driver=\"LXNAV\"\n"
                "Port1Enabled=\"1\"\n"
                "Port2Type=\"tcp_client\"\n"
                "Port2IPAddress=\"10.0.0.5\"\n"
                "Port2TCPPort=\"2000\"\n"
                "Port2Driver=\"FLARM\"\n"
                "Port2Enabled=\"1\"\n");

  std::array<DeviceConfig, NUMDEV> devices;
  ok1(DevicePorts::LoadLegacy(path, devices));

  ok1(devices[0].port_type == DeviceConfig::PortType::SERIAL);
  ok1(devices[0].path == StaticString<64>("/dev/ttyUSB0"));
  ok1(devices[0].baud_rate == 19200);
  ok1(devices[0].driver_name == StaticString<32>("LXNAV"));

  ok1(devices[1].port_type == DeviceConfig::PortType::TCP_CLIENT);
  ok1(devices[1].ip_address == StaticString<64>("10.0.0.5"));
  ok1(devices[1].tcp_port == 2000);
  ok1(devices[1].driver_name == StaticString<32>("FLARM"));

  /* and what was converted survives a save/load in the new format */
  ok1(DevicePorts::Save(devices));

  std::array<DeviceConfig, NUMDEV> loaded;
  ok1(DevicePorts::Load(loaded));
  ok1(loaded[0].driver_name == StaticString<32>("LXNAV"));
  ok1(loaded[1].driver_name == StaticString<32>("FLARM"));
  ok1(loaded[0].baud_rate == 19200);

  File::Delete(path);
}

/**
 * Only what differs from the default is written, and a setting put
 * back to its default disappears from the file again.
 */
static void
TestSparseFile()
{
  std::array<DeviceConfig, NUMDEV> devices;
  for (auto &i : devices)
    i.Clear();

  devices[0] = MakeConfig(DeviceConfig::PortType::SERIAL, "COM2:",
                          4800, "CAI302");
  devices[0].sync_to_device = false;

  ok1(DevicePorts::Save(devices));

  const auto text = ReadTextFile(DevicePorts::GetPath());

  /* the untouched slots are not in the file at all */
  ok1(text.find("\"index\": 1") == std::string::npos);

  /* neither is a value that equals the default: 4800 baud is what
     DeviceConfig::Clear() sets */
  ok1(text.find("baud_rate") == std::string::npos);
  ok1(text.find("i2c_bus") == std::string::npos);

  /* what was changed is there */
  ok1(text.find("CAI302") != std::string::npos);
  ok1(text.find("sync_to_device") != std::string::npos);

  /* and it still reads back as what was saved */
  std::array<DeviceConfig, NUMDEV> loaded;
  ok1(DevicePorts::Load(loaded));
  ok1(loaded[0].driver_name == StaticString<32>("CAI302"));
  ok1(loaded[0].baud_rate == 4800);
  ok1(loaded[0].i2c_bus == devices[0].i2c_bus);
  ok1(!loaded[0].sync_to_device);
  ok1(loaded[1].port_type == DeviceConfig::PortType::DISABLED);

  /* back to the default: the key has to go */
  devices[0].sync_to_device = true;
  ok1(DevicePorts::Save(devices));

  const auto text2 = ReadTextFile(DevicePorts::GetPath());
  ok1(text2.find("sync_to_device") == std::string::npos);

  std::array<DeviceConfig, NUMDEV> loaded2;
  ok1(DevicePorts::Load(loaded2));
  ok1(loaded2[0].sync_to_device);
  ok1(loaded2[0].driver_name == StaticString<32>("CAI302"));
}

int
main()
{
  char template_path[] = "/tmp/opensoar-ports-XXXXXX";
  if (mkdtemp(template_path) == nullptr)
    return 1;

  /* the device settings directory is derived from the data path */
  setenv("XDG_CONFIG_HOME", template_path, 1);
  SetSingleDataPath(Path{template_path});
  InitialiseDataPath();

  plan_tests(52);

  TestSaveLoadRoundTrip();
  TestSaveOverwrites();
  TestLegacyConversion();
  TestSparseFile();

  DeinitialiseDataPath();

  return exit_status();
}
