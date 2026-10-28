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
# include "Device/Features.hpp"  // the SteFly slots
# include "util/StringAPI.hxx"
# include "Device/Driver/SteFly/Discovery.hpp"
#endif

#ifdef HAVE_POSIX
#include "Device/Port/TTYEnumerator.hpp"
#endif

#ifdef _WIN32
# include "system/WindowsRegistry.hpp"
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

  { DeviceConfig::PortType::SPECTATE_FILE, N_("Condor 3 Spectate file") },

  { DeviceConfig::PortType::SERIAL, nullptr } /* sentinel */
};

/** the number of fixed port types (excludes Serial, Bluetooth and IOIOUart) */
static constexpr unsigned num_port_types = std::size(port_types) - 1;

/**
 * Has this port been claimed by one of the fixed SteFly slots (the
 * RemoteStick or the RotaryPanel, see Startup.cpp)?  The path is a COM
 * port on Windows, a tty on Linux or a UsbSerialHelper id on Android.
 *
 * The port-picker dropdown for the regular device slots skips such a
 * path so the user cannot accidentally re-select the same physical
 * port and race the SteFly descriptor.
 */
[[gnu::pure]]
static bool
IsSteFlyReserved([[maybe_unused]] const char *path) noexcept
{
#ifdef HAVE_REMOTE_STICK
  for (const auto &auto_slot : SteFly::AUTO_SLOTS) {
    const DeviceConfig &cfg =
      CommonInterface::GetSystemSettings().devices[auto_slot.slot];
    if (cfg.UsesPort() && !cfg.path.empty() &&
        StringIsEqual(path, cfg.path.c_str()))
      return true;
  }
#endif
  return false;
}

#if defined(HAVE_POSIX) && defined(HAVE_REMOTE_STICK)

/**
 * Is this device node the kernel's own tty of an attached SteFly
 * device whose slot is opened through another path?  On Android the
 * slot uses the UsbSerialHelper id, but some devices additionally
 * expose the device as /dev/ttyUSB* or /dev/ttyACM*; a regular slot
 * opened on that node would race the SteFly descriptor for the same
 * data.
 */
[[gnu::pure]]
static bool
IsSteFlyTty(const char *path) noexcept
{
  for (const auto &auto_slot : SteFly::AUTO_SLOTS)
    if (CommonInterface::GetSystemSettings().devices[auto_slot.slot].UsesPort() &&
        SteFly::IsUsbTty(path, SteFly::VID_STEFLY, auto_slot.pid))
      return true;
  return false;
}

#endif

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

  bool found = false;
  const char *path;
  while ((path = enumerator.Next()) != nullptr) {
    /* skip the ports the SteFly devices have been auto-bound to */
    if (IsSteFlyReserved(path))
      continue;

#ifdef HAVE_REMOTE_STICK
    if (IsSteFlyTty(path))
      continue;
#endif

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

/**
 * If this is a "COMx" name, parse "x" and return it; otherwise,
 * return -1.
 */
[[gnu::pure]]
static int
ComIndex(char *name) noexcept
{
  const char *suffix = StringAfterPrefix(name, "COM");
  if (suffix == nullptr)
    return -1;

  char *endptr;
  const auto i = strtoul(suffix, &endptr, 10);
  if (endptr == suffix || *endptr != '\0')
    return -1;

  return static_cast<int>(i);
}

static void
DetectSerialPorts(DataFieldEnum &df) noexcept
try {
  /* the registry key HKEY_LOCAL_MACHINE/Hardware/DEVICEMAP/SERIALCOMM
     is the best way to discover serial ports on Windows */

  RegistryKey hardware{HKEY_LOCAL_MACHINE, "Hardware"};
  RegistryKey devicemap{hardware, "DEVICEMAP"};
  RegistryKey serialcomm{devicemap, "SERIALCOMM"};

  for (unsigned i = 0;; ++i) {
    char name[128];
    char value[64];

    DWORD type;
    if (!serialcomm.EnumValue(i, std::span{name}, &type,
                              std::as_writable_bytes(std::span{value})))
      break;

    if (type != REG_SZ)
      // weird
      continue;

    const char *path = value;

    char buffer[128];
    if (const auto com_idx = ComIndex(value); com_idx >= 0) {
      if (com_idx < 10)
        /* old-style raw names (with trailing colon for backwards
           compatibility with older XCSoar versions) */
        StringFormatUnsafe(buffer, "%s:", value);
      else
        /* COM10 and above must use UNC paths */
        StringFormatUnsafe(buffer, "\\\\.\\%s", value);
      path = buffer;
    }

    /* skip the ports the SteFly devices have been auto-bound to; their
       slots hold the path in the same spelling as built above */
    if (IsSteFlyReserved(path))
      continue;

    AddPort(df, DeviceConfig::PortType::SERIAL, path, name);
  }
} catch (const std::system_error &) {
  // silently ignore registry errors
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

void
SetPort(DataFieldEnum &df, DeviceConfig::PortType type,
        const char *value) noexcept
{
  assert(value != nullptr);

  if (!df.SetValue(value))
    df.SetValue(AddPort(df, type, value));
}

static void
FillSerialPorts(DataFieldEnum &df, const DeviceConfig &config) noexcept
{
#if defined(HAVE_POSIX) || defined(_WIN32)
  DetectSerialPorts(df);
#endif

  if (config.port_type == DeviceConfig::PortType::SERIAL)
    SetPort(df, config.port_type, config.path);
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
FillAndroidUsbSerialPorts([[maybe_unused]] DataFieldEnum &df,
                          [[maybe_unused]] const DeviceConfig &config) noexcept
{
#ifdef ANDROID
  if (config.port_type == DeviceConfig::PortType::ANDROID_USB_SERIAL &&
      !config.path.empty())
    SetPort(df, DeviceConfig::PortType::ANDROID_USB_SERIAL, config.path);
#endif
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
  FillAndroidUsbSerialPorts(df, config);
  FillAndroidIOIOPorts(df, config);
}

void
UpdatePortEntry(DataFieldEnum &df, DeviceConfig::PortType type,
                const char *value, const char *name) noexcept
{
  // The Android port picker reports every USB serial device it sees,
  // including the SteFly devices that their fixed slots already own.
  // Offering one for a regular slot would let two descriptors fight
  // over one device, so they are left out like on the serial list.
  if (type == DeviceConfig::PortType::ANDROID_USB_SERIAL &&
      IsSteFlyReserved(value))
    return;

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

void
SetPort(DataFieldEnum &df, const DeviceConfig &config) noexcept
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
  case DeviceConfig::PortType::SPECTATE_FILE:
    break;

  case DeviceConfig::PortType::SERIAL:
  case DeviceConfig::PortType::ANDROID_USB_SERIAL:
    SetPort(df, config.port_type, config.path);
    return;

  case DeviceConfig::PortType::BLE_SENSOR:
  case DeviceConfig::PortType::BLE_SERIAL:
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
