// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "PortDataField.hpp"
#include "Device/Features.hpp"
#include "Form/DataField/Enum.hpp"
#include "Language/Language.hpp"
#include "util/StringCompare.hxx"

#ifdef HAVE_REMOTE_STICK
# include "Interface.hpp"
# include "SystemSettings.hpp"
#endif

#ifdef HAVE_POSIX
#include "Device/Port/TTYEnumerator.hpp"
#endif

#ifdef _WIN32
# include "Device/Port/WindowsSerialPorts.hpp"
# include "LogFile.hpp"
# include "util/StringFormat.hpp"
#endif

#ifdef ANDROID
#include "java/Global.hxx"
#include "java/String.hxx"
#include "Android/Main.hpp"
#include "Android/BluetoothHelper.hpp"
#include "Android/UsbSerialHelper.hpp"
#include "Device/Port/AndroidIOIOUartPort.hpp"
#endif

static constexpr struct {
  DeviceConfig::PortType type;
  const char *label;
} port_types[] = {
  { DeviceConfig::PortType::DISABLED, N_("Disabled") },
#ifdef HAVE_INTERNAL_GPS
  { DeviceConfig::PortType::INTERNAL, N_("Built-in GPS & sensors") },
#endif
#ifdef ANDROID
  { DeviceConfig::PortType::RFCOMM_SERVER, N_("Bluetooth server") },
  { DeviceConfig::PortType::DROIDSOAR_V2, "DroidSoar V2" },
  { DeviceConfig::PortType::GLIDER_LINK, "GliderLink traffic receiver" },
#ifndef NDEBUG
  { DeviceConfig::PortType::NUNCHUCK, N_("IOIO switches and Nunchuk") },
#endif
  { DeviceConfig::PortType::I2CPRESSURESENSOR, N_("IOIO I²C pressure sensor") },
  { DeviceConfig::PortType::IOIOVOLTAGE, N_("IOIO voltage sensor") },
#endif

  { DeviceConfig::PortType::TCP_CLIENT, N_("TCP client") },

  /* label not translated for now, until we have a TCP/UDP port
     selection UI */
  { DeviceConfig::PortType::TCP_LISTENER, N_("TCP port") },
  { DeviceConfig::PortType::UDP_LISTENER, N_("UDP port") },

  { DeviceConfig::PortType::SERIAL, nullptr } /* sentinel */
};

/** the number of fixed port types (excludes Serial, Bluetooth and IOIOUart) */
static constexpr unsigned num_port_types = std::size(port_types) - 1;

/**
 * Return the COM / tty path that has been claimed by the fixed
 * SteFly RemoteStick slot at REMOTE_PORT (see Startup.cpp), or
 * nullptr if no RemoteStick was auto-detected this session.
 *
 * The port-picker dropdown for the other six device slots skips
 * this path so the user cannot accidentally re-select the same
 * physical port and race the RemoteStick descriptor. On non-
 * RemoteStick builds this is a compile-time nullptr.
 */
[[gnu::pure]]
static const char *
GetStePortReserved() noexcept
{
#ifdef HAVE_REMOTE_STICK
  const DeviceConfig &cfg =
    CommonInterface::GetSystemSettings().devices[REMOTE_PORT];
  if (cfg.UsesPort() && !cfg.path.empty())
    return cfg.path.c_str();
#endif
  return nullptr;
}

static unsigned
AddPort(DataFieldEnum &df, DeviceConfig::PortType type,
        const char *text, const char *display_string=nullptr,
        const char *help=nullptr) noexcept
{
  /* the upper 16 bit is the port type, and the lower 16 bit is a
     serial number to make the enum id unique */

  unsigned id = ((unsigned)type << 16) + df.Count();
  df.AddChoice(id, text, display_string, help);
  return id;
}

#if defined(HAVE_POSIX)

static bool
DetectSerialPorts(DataFieldEnum &df) noexcept
{
  TTYEnumerator enumerator;
  if (enumerator.HasFailed())
    return false;

  unsigned sort_start = df.Count();

  // Skip the port that the SteFly RemoteStick has been auto-bound
  // to (REMOTE_PORT slot); nullptr on non-RemoteStick builds and
  // when nothing was auto-detected.
  const char *const reserved = GetStePortReserved();

  bool found = false;
  const char *path;
  while ((path = enumerator.Next()) != nullptr) {
    if (reserved != nullptr && StringIsEqual(path, reserved))
      continue;

    const char *display_string = StringAfterPrefix(path, "/dev/");
    if (display_string == nullptr)
      display_string = path;

    AddPort(df, DeviceConfig::PortType::SERIAL, path, display_string);
    found = true;
  }

  if (found)
    df.Sort(sort_start);

  return found;
}

#elif defined(_WIN32)

static void
DetectSerialPorts(DataFieldEnum &df) noexcept
{
  const auto e = EnumerateSerialPorts();

  if (!e.have_serialcomm) {
    LogString("PortPicker: no serial ports "
              "(Hardware\\DeviceMap\\SerialComm is missing)");
    return;
  }

  if (!e.have_arbiter)
    LogString("PortPicker: COM Name Arbiter not readable, "
              "device classes are unknown");

  // Skip the port that the SteFly RemoteStick has been auto-bound
  // to (REMOTE_PORT slot); nullptr on non-RemoteStick builds and
  // when nothing was auto-detected.
  const char *const reserved = GetStePortReserved();

  for (const auto &port : e.ports) {
    LogFormat("PortPicker: %s (%s) class=%s%s%s", port.path.c_str(),
              port.device_path.c_str(),
              port.arbiter.empty() ? "unknown" : port.arbiter.c_str(),
              port.hidden ? " hidden: " : "",
              port.hidden ? port.hidden_reason : "");

    if (port.hidden)
      continue;

    if (reserved != nullptr && StringIsEqual(port.path.c_str(), reserved))
      continue;

    AddPort(df, port.type, port.path.c_str(), port.display.c_str());
  }
}

#endif

static void
FillPortTypes(DataFieldEnum &df, const DeviceConfig &config) noexcept
{
  for (unsigned i = 0; port_types[i].label != nullptr; i++) {
    unsigned id = AddPort(df, port_types[i].type, port_types[i].label,
                          gettext(port_types[i].label));

    if (port_types[i].type == config.port_type)
      df.SetValue(id);
  }
}

// this function is only used inside this source file.
// maybe this is to check with the tests!
static void
SetPort(DataFieldEnum &df, DeviceConfig::PortType type,
        const char *value) noexcept
{
  assert(value != nullptr);

  if (!df.SetValue(value))
    df.SetValue(AddPort(df, type, value));
}

static void FillSerialPorts(DataFieldEnum &df,
                            const DeviceConfig &config) noexcept {
  DetectSerialPorts(df);

  switch (config.port_type) {
  case DeviceConfig::PortType::SERIAL:
  case DeviceConfig::PortType::RFCOMM:
  case DeviceConfig::PortType::BLE_HM10:
  case DeviceConfig::PortType::BLE_SENSOR:
  case DeviceConfig::PortType::USB_SERIAL:
    SetPort(df, config.port_type, config.path);
  default:
    break;
  }
}

void
SetBluetoothPort(DataFieldEnum &df, DeviceConfig::PortType type,
                 const char *bluetooth_mac) noexcept
{
  assert(bluetooth_mac != nullptr);

  if (!df.SetValue(bluetooth_mac)) {
    const char *name = nullptr;
#ifdef ANDROID
    if (bluetooth_helper != nullptr)
      name = bluetooth_helper->GetNameFromAddress(Java::GetEnv(),
                                                  bluetooth_mac);
#endif
    df.SetValue(AddPort(df, type, bluetooth_mac, name));
  }
}

static void
FillAndroidBluetoothPorts(DataFieldEnum &df,
                          const DeviceConfig &config) noexcept
{
  if (config.UsesBluetoothMac() &&
      !config.bluetooth_mac.empty())
    SetBluetoothPort(df, config.port_type, config.bluetooth_mac);
}

static void
FillAndroidIOIOPorts([[maybe_unused]] DataFieldEnum &df, [[maybe_unused]] const DeviceConfig &config) noexcept
{
#if defined(ANDROID)
  df.EnableItemHelp(true);

  char tempID[4];
  char tempName[15];
  for (unsigned i = 0; i < AndroidIOIOUartPort::getNumberUarts(); i++) {
    StringFormatUnsafe(tempID, "%u", i);
    StringFormat(tempName, sizeof(tempName), "IOIO UART %u", i);
    unsigned id = AddPort(df, DeviceConfig::PortType::IOIOUART,
                          tempID, tempName,
                          AndroidIOIOUartPort::getPortHelp(i));
    if (config.port_type == DeviceConfig::PortType::IOIOUART &&
        config.ioio_uart_id == i)
      df.SetValue(id);
  }
#endif
}

void
FillPorts(DataFieldEnum &df, const DeviceConfig &config) noexcept
{
  FillPortTypes(df, config);
  FillSerialPorts(df, config);
  FillAndroidBluetoothPorts(df, config);
  FillAndroidIOIOPorts(df, config);
}

void
UpdatePortEntry(DataFieldEnum &df, DeviceConfig::PortType type,
                const char *value, const char *name) noexcept
{
  for (std::size_t i = 0, n = df.Count(); i < n; ++i) {
    const auto &item = df[i];
    if (DeviceConfig::PortType(item.GetId() >> 16) == type &&
        StringIsEqual(value, item.GetString())) {
      if (name != nullptr)
        df.SetDisplayString(i, name);
      return;
    }
  }

  AddPort(df, type, value, name);
}

/*
with some compiler and constellations the function name 'SetPort' makes
problems during the linking:
SetPort(DataFieldEnum&, const DeviceConfig&) not found...
After renaming to SetDevicePort this problem disappears.
*/
void
SetDevicePort(DataFieldEnum &df, const DeviceConfig &config) noexcept
{
  switch (config.port_type) {
  case DeviceConfig::PortType::DISABLED:
  case DeviceConfig::PortType::AUTO:
  case DeviceConfig::PortType::INTERNAL:
  case DeviceConfig::PortType::DROIDSOAR_V2:
  case DeviceConfig::PortType::NUNCHUCK:
  case DeviceConfig::PortType::I2CPRESSURESENSOR:
  case DeviceConfig::PortType::IOIOVOLTAGE:
  case DeviceConfig::PortType::TCP_CLIENT:
  case DeviceConfig::PortType::TCP_LISTENER:
  case DeviceConfig::PortType::UDP_LISTENER:
  case DeviceConfig::PortType::PTY:
  case DeviceConfig::PortType::RFCOMM_SERVER:
  case DeviceConfig::PortType::GLIDER_LINK:
    break;

  case DeviceConfig::PortType::SERIAL:
  case DeviceConfig::PortType::USB_SERIAL:
    SetPort(df, config.port_type, config.path);
    return;

  case DeviceConfig::PortType::BLE_SENSOR:
  case DeviceConfig::PortType::BLE_HM10:
  case DeviceConfig::PortType::RFCOMM:
    SetBluetoothPort(df, config.port_type, config.bluetooth_mac);
    return;

  case DeviceConfig::PortType::IOIOUART:
    StaticString<16> buffer;
    buffer.UnsafeFormat("%d", config.ioio_uart_id);
    df.SetValue(buffer);
    return;
  }

  for (unsigned i = 0; port_types[i].label != nullptr; i++) {
    if (port_types[i].type == config.port_type) {
      df.SetValue(port_types[i].label);
      break;
    }
  }
}

DeviceConfig::PortType
GetPortType(const DataFieldEnum &df) noexcept
{
  const unsigned port = df.GetValue();

  if (port < num_port_types)
    return port_types[port].type;

  return (DeviceConfig::PortType)(port >> 16);
}
