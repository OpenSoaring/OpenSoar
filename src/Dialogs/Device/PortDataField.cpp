// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "PortDataField.hpp"
#include "Device/Features.hpp"
#include "Form/DataField/Enum.hpp"
#include "Language/Language.hpp"
#include "util/StringCompare.hxx"

#ifdef HAVE_POSIX
#include "Device/Port/TTYEnumerator.hpp"
#endif

#ifdef _WIN32
# include "system/WindowsRegistry.hpp"
# include "util/StringFormat.hpp"
# include <boost/algorithm/string.hpp>
# include <map>
  typedef std::string tstring;
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
  const TCHAR *label;
} port_types[] = {
  { DeviceConfig::PortType::DISABLED, N_("Disabled") },
#ifdef HAVE_INTERNAL_GPS
  { DeviceConfig::PortType::INTERNAL, N_("Built-in GPS & sensors") },
#endif
#ifdef ANDROID
  { DeviceConfig::PortType::RFCOMM_SERVER, N_("Bluetooth server") },
  { DeviceConfig::PortType::DROIDSOAR_V2, _T("DroidSoar V2") },
  { DeviceConfig::PortType::GLIDER_LINK, _T("GliderLink traffic receiver") },
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

static unsigned
AddPort(DataFieldEnum &df, DeviceConfig::PortType type,
        const TCHAR *text, const TCHAR *display_string=nullptr,
        const TCHAR *help=nullptr) noexcept
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
try {
//-------------------------------
  RegistryKey bthenums{HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\"
                                              "Enum\\BthEnum")};
  std::map<const tstring, tstring> bthmap;
  for (unsigned k = 0;; ++k) {
    TCHAR dev_name[128];
    TCHAR name[128];
    TCHAR friendly_name[128];

    if (!bthenums.EnumKey(k, std::span{dev_name}))
      break;
    tstring map_name(dev_name);
    if (!map_name.starts_with(_T("Dev_")))
      continue;
    RegistryKey bthenum_dev{bthenums, dev_name};
    // only one is possible...
    if (!bthenum_dev.EnumKey(0, std::span{name}))
      break;
    RegistryKey bthenum_key{bthenum_dev, name};
    if (!bthenum_key.GetValue(_T("FriendlyName"), friendly_name))
      break;
    map_name = map_name.substr(4);
    for (tstring::iterator it = map_name.begin(); it != map_name.end();
         ++it)
      *it = towlower(*it);
    bthmap[map_name] = friendly_name; // Left "Dev_"
  }
  RegistryKey bthle{HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\"
                                              "Enum\\BthLE")};
  std::map<const tstring, tstring> blemap;
  for (unsigned k = 0;; ++k) {
    TCHAR dev_name[128];
    TCHAR name[128];
    TCHAR friendly_name[128];

    if (!bthle.EnumKey(k, std::span{dev_name}))
      break;
    tstring map_name(dev_name);
    if (!map_name.starts_with(_T("Dev_")))
      continue;
    RegistryKey bthle_dev{bthle, dev_name};
    // only one is possible...
    if (!bthle_dev.EnumKey(0, std::span{name}))
      break;
    RegistryKey bthle_key{bthle_dev, name};
    if (!bthle_key.GetValue(_T("FriendlyName"), friendly_name))
      break;
    map_name = map_name.substr(4);
    for (tstring::iterator it = map_name.begin(); it != map_name.end();
         ++it)
      *it = towlower(*it);
    blemap[map_name] = friendly_name; // Left "Dev_"
  }

  /* the registry key HKEY_LOCAL_MACHINE/Hardware/DEVICEMAP/SERIALCOMM
     is the best way to discover serial ports on Windows */
  RegistryKey serialcomm{HKEY_LOCAL_MACHINE, _T("Hardware\\DeviceMap\\SerialComm")};

  for (unsigned i = 0;; ++i) {
    TCHAR name[128];
    TCHAR value[64];

    DWORD type;
    if (!serialcomm.EnumValue(i, std::span{name}, &type,
                              std::as_writable_bytes(std::span{value})))
      break;

    if (type != REG_SZ)
      // weird
      continue;

    RegistryKey devices {HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\"
            "Control\\COM Name Arbiter\\Devices")};
    TCHAR name1[0x200];

    if (!devices.GetValue(value, name1))
      break;

    // Registry "COM Name Arbiter\Devices" starts with
    // BlueTooth: "\\?\bthenum#"
    // USB:       "\\?\usb#"
    // Normal:    "\\?\acpi#"   - an Kupschis Rechner!
    tstring dev = name1;
    if (dev.starts_with(_T("\\\\?\\usb#"))) {
      std::vector<tstring> strs;
      tstring port_name;
      boost::split(strs, name, boost::is_any_of("\\"));
      port_name = value;
      port_name += _T(" (");
      port_name += strs[2];
      port_name += _T(")");

      AddPort(df, DeviceConfig::PortType::USB_SERIAL, value, port_name.c_str());
    } else if (dev.starts_with(_T("\\\\?\\bthenum#"))) {
      std::vector<tstring> strs;
      tstring port_name;
      boost::split(strs, name1, boost::is_any_of("#"));
      boost::split(strs, strs[2], boost::is_any_of("_"));
      if (strs[1] == _T("c00000000")) {
        boost::split(strs, strs[0], boost::is_any_of("&"));
        if (strs[3] != _T("000000000000")) {
          DeviceConfig::PortType port_type = DeviceConfig::PortType::DISABLED;
          port_name = value;
          port_name += _T(" (");
          if (blemap.find(strs[3]) != blemap.end()) {
            port_type = DeviceConfig::PortType::RFCOMM;
            port_type = DeviceConfig::PortType::BLE_HM10;
            // port_type = DeviceConfig::PortType::BLE_SENSOR;
            port_name += blemap[strs[3]];
          } else if (bthmap.find(strs[3]) != bthmap.end()) {
            port_type = DeviceConfig::PortType::RFCOMM;
            port_name += bthmap[strs[3]];
          } 

          port_name += _T(")");
          if (port_type != DeviceConfig::PortType::DISABLED)
            AddPort(df, port_type, value,
                   port_name.c_str());
        }
      }
    } else //  if (dev.starts_with(_T("\\\\?\\acpi#")))
      AddPort(df, DeviceConfig::PortType::SERIAL, value, name);
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

// this function is only used inside this source file.
// maybe this is to check with the tests!
static void
SetPort(DataFieldEnum &df, DeviceConfig::PortType type,
        const TCHAR *value) noexcept
{
  assert(value != nullptr);

  if (!df.SetValue(value))
    df.SetValue(AddPort(df, type, value));
}

static void FillSerialPorts(DataFieldEnum &df,
                            const DeviceConfig &config) noexcept {
#if defined(HAVE_POSIX) || defined(_WIN32)
  DetectSerialPorts(df);
#endif

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
                 const TCHAR *bluetooth_mac) noexcept
{
  assert(bluetooth_mac != nullptr);

  if (!df.SetValue(bluetooth_mac)) {
    const TCHAR *name = nullptr;
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
  if (config.port_type == DeviceConfig::PortType::USB_SERIAL &&
      !config.path.empty())
    SetPort(df, DeviceConfig::PortType::USB_SERIAL, config.path);
#endif
}

static void
FillAndroidIOIOPorts([[maybe_unused]] DataFieldEnum &df, [[maybe_unused]] const DeviceConfig &config) noexcept
{
#if defined(ANDROID)
  df.EnableItemHelp(true);

  TCHAR tempID[4];
  TCHAR tempName[15];
  for (unsigned i = 0; i < AndroidIOIOUartPort::getNumberUarts(); i++) {
    StringFormatUnsafe(tempID, _T("%u"), i);
    StringFormat(tempName, sizeof(tempName), _T("IOIO UART %u"), i);
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
                const TCHAR *value, const TCHAR *name) noexcept
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
    buffer.UnsafeFormat(_T("%d"), config.ioio_uart_id);
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
