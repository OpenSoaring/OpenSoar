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
# include "system/WindowsRegistry.hpp"
# include "LogFile.hpp"
# include "util/StringFormat.hpp"
# include <boost/algorithm/string.hpp>
# include <map>
# include <optional>
# include <string>
# include <vector>
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

/**
 * Open a registry key.  Unlike the RegistryKey constructor, this does
 * not throw when the key is missing or unreadable - a key which was
 * never created (e.g. BthLE without a single paired BLE device) is a
 * normal condition, not an error.
 */
static std::optional<RegistryKey>
TryOpenRegistryKey(HKEY parent, const char *key) noexcept
try {
  return RegistryKey{parent, key};
} catch (const std::system_error &) {
  return std::nullopt;
}

/**
 * Collect "device address" -> "friendly name" from one of the
 * Bluetooth enumeration keys.  Failures are per-device: one unreadable
 * entry must not cost us the whole map.
 */
static std::map<std::string, std::string>
CollectBluetoothNames(const char *key_name) noexcept
{
  std::map<std::string, std::string> map;

  const auto key = TryOpenRegistryKey(HKEY_LOCAL_MACHINE, key_name);
  if (!key) {
    LogFormat("PortPicker: registry key %s not available", key_name);
    return map;
  }

  for (unsigned k = 0;; ++k) {
    char dev_name[128], name[128], friendly_name[128];

    if (!key->EnumKey(k, std::span{dev_name}))
      break;

    std::string map_name{dev_name};
    if (!map_name.starts_with("Dev_"))
      continue;

    const auto dev = TryOpenRegistryKey(*key, dev_name);
    if (!dev || !dev->EnumKey(0, std::span{name}))
      continue;

    const auto sub = TryOpenRegistryKey(*dev, name);
    if (!sub || !sub->GetValue("FriendlyName", std::span{friendly_name}))
      continue;

    map_name = map_name.substr(4);
    for (auto &ch : map_name)
      if (ch >= 'A' && ch <= 'Z')
        ch += 'a' - 'A';

    map[map_name] = friendly_name;
  }

  return map;
}

/**
 * A port which is only offered when nothing better turned up.
 */
struct PendingPort {
  DeviceConfig::PortType type;
  std::string value, display;
};

/**
 * Classify one entry of Hardware\DeviceMap\SerialComm and add it to
 * the list.  Every port ends up in the list; when the device class
 * cannot be determined, it is offered as a plain serial port rather
 * than being dropped.
 */
static void
AddDetectedPort(DataFieldEnum &df, const char *value, const char *name,
                const std::string &dev_name,
                const std::map<std::string, std::string> &bthmap,
                const std::map<std::string, std::string> &blemap,
                unsigned &n_bluetooth,
                std::vector<PendingPort> &deferred) noexcept
{
  std::vector<std::string> strs;
  std::string portname{value};
  const std::string device_path{name};

  if (dev_name.starts_with("\\\\?\\usb#")) {
    boost::split(strs, device_path, boost::is_any_of("\\"));

    if (strs.size() > 2 && strs[2].starts_with("USBSER")) {
      std::vector<std::string> dev;
      boost::split(dev, dev_name, boost::is_any_of("#"));

      char friendly_name[0x100];
      std::optional<RegistryKey> usb_devices;

      if (dev.size() > 2) {
        std::string usb_device = "SYSTEM\\CurrentControlSet\\ENUM\\USB\\";
        usb_device += dev[1]; // e.g. "vid_1209&pid_8500&mi_00"
        usb_device += "\\";
        usb_device += dev[2]; // e.g. "7&3226aff9&0&0000"
        usb_devices = TryOpenRegistryKey(HKEY_LOCAL_MACHINE,
                                         usb_device.c_str());
      }

      if (usb_devices &&
          usb_devices->GetValue("FriendlyName", std::span{friendly_name}))
        portname = friendly_name; // e.g. "SteFly Stick", "Arduino"
      else
        portname += " (USB)";
    } else if (strs.size() > 2) {
      portname += " (";
      portname += strs[2];
      portname += ")";
    }

    AddPort(df, DeviceConfig::PortType::USB_SERIAL, value, portname.c_str());
    return;
  }

  if (dev_name.starts_with("\\\\?\\root#")) {
    boost::split(strs, device_path, boost::is_any_of("\\"));
    if (strs.size() > 2) {
      portname += " (";
      portname += strs[2];
      portname += ")";
    }

    AddPort(df, DeviceConfig::PortType::SERIAL, value, portname.c_str());
    return;
  }

  if (dev_name.starts_with("\\\\?\\bthenum#")) {
    /* the instance path looks like
       \\?\bthenum#{0000...}_localmfg&005d#9&2566f247&0&c049ef635562_c00000000#{...}
       where the field before the trailing "_" is the address of the
       remote device, and the field after it marks the direction:
       "c00000000" for the outgoing port, "00000000" for the local
       listener which Windows creates alongside it */
    std::string address;

    boost::split(strs, dev_name, boost::is_any_of("#"));
    if (strs.size() > 2) {
      /* split into a separate vector: boost::split() clears its
         output container first, so splitting strs into strs would
         read from elements it has already destroyed */
      std::vector<std::string> parts;
      boost::split(parts, strs[2], boost::is_any_of("_"));

      if (parts.size() > 1) {
        std::vector<std::string> fields;
        boost::split(fields, parts[0], boost::is_any_of("&"));
        if (fields.size() > 3)
          address = fields[3];
      }
    }

    if (address == "000000000000") {
      /* no remote device: this is the incoming port, waiting for the
         device to call us, which none of our devices does.  All
         traffic goes through the outgoing port, so keep this one out
         of the way - unless it turns out to be all we have */
      deferred.push_back({DeviceConfig::PortType::RFCOMM, value,
                          std::string{value} + " (" + _("incoming") + ")"});
      return;
    }

    ++n_bluetooth;

    if (!address.empty()) {
      if (const auto ble = blemap.find(address); ble != blemap.end()) {
        portname += " (" + ble->second + ")";
        AddPort(df, DeviceConfig::PortType::BLE_HM10, value,
                portname.c_str());
        return;
      }

      if (const auto bth = bthmap.find(address); bth != bthmap.end()) {
        portname += " (" + bth->second + ")";
        AddPort(df, DeviceConfig::PortType::RFCOMM, value,
                portname.c_str());
        return;
      }

      /* a remote device we have no name for; show the address, which
         the user can look up in the Windows Bluetooth settings */
      portname += " (" + address + ")";
    }

    AddPort(df, DeviceConfig::PortType::RFCOMM, value, portname.c_str());
    return;
  }

  AddPort(df, DeviceConfig::PortType::SERIAL, value,
          dev_name.empty() ? value : name);
}

static void
DetectSerialPorts(DataFieldEnum &df) noexcept
{
  /* the friendly names are optional; a missing Bluetooth key must not
     cost us the serial ports */
  const auto bthmap =
    CollectBluetoothNames("SYSTEM\\CurrentControlSet\\Enum\\BthEnum");
  const auto blemap =
    CollectBluetoothNames("SYSTEM\\CurrentControlSet\\Enum\\BthLE");

  /* the registry key HKEY_LOCAL_MACHINE/Hardware/DEVICEMAP/SERIALCOMM
     is the best way to discover serial ports on Windows */
  const auto serialcomm =
    TryOpenRegistryKey(HKEY_LOCAL_MACHINE, "Hardware\\DeviceMap\\SerialComm");
  if (!serialcomm) {
    LogString("PortPicker: no serial ports "
              "(Hardware\\DeviceMap\\SerialComm is missing)");
    return;
  }

  const auto com_devices =
    TryOpenRegistryKey(HKEY_LOCAL_MACHINE,
                       "SYSTEM\\CurrentControlSet\\Control\\"
                       "COM Name Arbiter\\Devices");
  if (!com_devices)
    LogString("PortPicker: COM Name Arbiter not readable, "
              "device classes are unknown");

  // Skip the port that the SteFly RemoteStick has been auto-bound
  // to (REMOTE_PORT slot); nullptr on non-RemoteStick builds and
  // when nothing was auto-detected.
  const char *const reserved = GetStePortReserved();

  /* Bluetooth ports which are only offered if nothing better shows
     up, so that this filter can never leave the user without a port */
  std::vector<PendingPort> deferred;
  unsigned n_bluetooth = 0;

  for (unsigned i = 0;; ++i) {
    char name[128];
    char value[64];

    DWORD type;
    if (!serialcomm->EnumValue(i, std::span{name}, &type,
                               std::as_writable_bytes(std::span{value})))
      break;  //  end of the list

    if (type != REG_SZ)
      // weird
      continue;

    // value holds the COM name (e.g. "COM7") - same shape as what
    // SteFly::Discovery returned, so a plain string compare is
    // enough.
    if (reserved != nullptr && StringIsEqual(value, reserved))
      continue;

    /* Registry "COM Name Arbiter\Devices" tells the device class:
       BlueTooth: "\\?\bthenum#", USB: "\\?\usb#",
       root: "\\?\root#" (virtual port), normal: "\\?\acpi#" */
    char arbiter[0x200];
    std::string dev_name;
    if (com_devices && com_devices->GetValue(value, std::span{arbiter}))
      dev_name = arbiter;

    LogFormat("PortPicker: serial %s (%s) class=%s", value, name,
              dev_name.empty() ? "unknown" : dev_name.c_str());

    AddDetectedPort(df, value, name, dev_name, bthmap, blemap,
                    n_bluetooth, deferred);
  }

  if (n_bluetooth == 0)
    /* no usable Bluetooth port was found, so offer the doubtful ones
       after all */
    for (const auto &i : deferred) {
      LogFormat("PortPicker: no other Bluetooth port, offering %s",
                i.value.c_str());
      AddPort(df, i.type, i.value.c_str(), i.display.c_str());
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
