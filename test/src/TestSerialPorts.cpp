// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Device/Port/SerialPortClassify.hpp"
#include "TestUtil.hpp"
#include "util/StringAPI.hxx"

/* the class strings below are verbatim registry values from a real
   machine, so this test keeps working without any port being present -
   which is what makes it usable on a build server */

static constexpr const char *BT_OUTGOING =
  "\\\\?\\bthenum#{00001101-0000-1000-8000-00805f9b34fb}_localmfg&005d"
  "#9&2566f247&0&c049ef635562_c00000000#{86e0d1e0-8089-11d0-9ce4-08003e301f73}";

static constexpr const char *BT_INCOMING =
  "\\\\?\\bthenum#{00001101-0000-1000-8000-00805f9b34fb}_localmfg&0000"
  "#9&2566f247&0&000000000000_00000000#{86e0d1e0-8089-11d0-9ce4-08003e301f73}";

static constexpr const char *USB_SERIAL =
  "\\\\?\\usb#vid_1209&pid_8500&mi_00#7&3226aff9&0&0000"
  "#{86e0d1e0-8089-11d0-9ce4-08003e301f73}";

static constexpr const char *VIRTUAL =
  "\\\\?\\root#ports#0000#{86e0d1e0-8089-11d0-9ce4-08003e301f73}";

static constexpr const char *ON_BOARD =
  "\\\\?\\acpi#pnp0501#1#{86e0d1e0-8089-11d0-9ce4-08003e301f73}";

int main()
{
  plan_tests(26);

  BluetoothNameMap classic, le;
  classic.emplace("c049ef635562", "Larus");

  /* the outgoing port of a paired device we know the name of */
  {
    const auto p = ClassifySerialPort("COM10", "\\Device\\BthModem0",
                                      BT_OUTGOING, classic, le);
    ok1(p.type == DeviceConfig::PortType::RFCOMM);
    ok1(StringIsEqual(p.address.c_str(), "c049ef635562"));
    ok1(StringIsEqual(p.display.c_str(), "COM10 (Larus)"));
    ok1(!p.hidden);
  }

  /* the local listener Windows creates alongside it */
  {
    const auto p = ClassifySerialPort("COM11", "\\Device\\BthModem1",
                                      BT_INCOMING, classic, le);
    ok1(p.type == DeviceConfig::PortType::RFCOMM);
    ok1(StringIsEqual(p.address.c_str(), "000000000000"));
    ok1(p.hidden);
    ok1(p.hidden_reason != nullptr);
  }

  /* a paired device which is not in the name map: the address is the
     next best label, and the port must not disappear */
  {
    BluetoothNameMap empty;
    const auto p = ClassifySerialPort("COM10", "\\Device\\BthModem0",
                                      BT_OUTGOING, empty, empty);
    ok1(p.type == DeviceConfig::PortType::RFCOMM);
    ok1(!p.hidden);
    ok1(StringIsEqual(p.display.c_str(), "COM10 (c049ef635562)"));
  }

  /* the same device known as a Bluetooth LE device */
  {
    BluetoothNameMap empty, ble;
    ble.emplace("c049ef635562", "Larus BLE");
    const auto p = ClassifySerialPort("COM10", "\\Device\\BthModem0",
                                      BT_OUTGOING, empty, ble);
    ok1(p.type == DeviceConfig::PortType::BLE_HM10);
    ok1(StringIsEqual(p.display.c_str(), "COM10 (Larus BLE)"));
  }

  /* USB, virtual and on-board ports */
  {
    const auto p = ClassifySerialPort("COM3", "\\Device\\USBSER000",
                                      USB_SERIAL, classic, le);
    ok1(p.type == DeviceConfig::PortType::USB_SERIAL);
    ok1(!p.hidden);
    ok1(StringIsEqual(p.display.c_str(), "COM3 (USB)"));
  }

  {
    const auto p = ClassifySerialPort("COM5", "\\Device\\Serial0",
                                      VIRTUAL, classic, le);
    ok1(p.type == DeviceConfig::PortType::SERIAL);
    ok1(StringIsEqual(p.display.c_str(), "COM5 (Serial0)"));
  }

  {
    const auto p = ClassifySerialPort("COM1", "\\Device\\Serial1",
                                      ON_BOARD, classic, le);
    ok1(p.type == DeviceConfig::PortType::SERIAL);
    ok1(StringIsEqual(p.display.c_str(), "\\Device\\Serial1"));
  }

  /* no class information at all: still a usable serial port */
  {
    const auto p = ClassifySerialPort("COM7", "\\Device\\Serial7", "",
                                      classic, le);
    ok1(p.type == DeviceConfig::PortType::SERIAL);
    ok1(StringIsEqual(p.display.c_str(), "COM7"));
  }

  /* the incoming port stays hidden while a usable one is present */
  {
    std::vector<DetectedSerialPort> ports;
    ports.push_back(ClassifySerialPort("COM10", "\\Device\\BthModem0",
                                       BT_OUTGOING, classic, le));
    ports.push_back(ClassifySerialPort("COM11", "\\Device\\BthModem1",
                                       BT_INCOMING, classic, le));
    ResolveHiddenPorts(ports);
    ok1(!ports[0].hidden);
    ok1(ports[1].hidden);
  }

  /* ... but it is offered when it is all we have */
  {
    std::vector<DetectedSerialPort> ports;
    ports.push_back(ClassifySerialPort("COM11", "\\Device\\BthModem1",
                                       BT_INCOMING, classic, le));
    ResolveHiddenPorts(ports);
    ok1(!ports[0].hidden);
    ok1(ports[0].hidden_reason == nullptr);
  }

  return exit_status();
}
