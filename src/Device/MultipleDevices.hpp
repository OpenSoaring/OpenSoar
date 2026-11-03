// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

/** \file
 *
 * This library manages the list of configured devices.
 */

#pragma once

#include "Features.hpp"
#include "Device/Port/Listener.hpp"
#include "thread/Mutex.hxx"
#include "Blackboard/DeviceBlackboard.hpp"

#include <optional>

#include <array>
#include <atomic>
#include <list>
class DeviceBlackboard;
class NMEALogger;
class DeviceFactory;
class DeviceDescriptor;
class DeviceDispatcher;
struct MoreData;
struct DerivedInfo;
class GlidePolar;
struct GeoPoint;
class AtmosphericPressure;
class RadioFrequency;
class TransponderCode;
class OperationEnvironment;

/**
 * Container for all (configured) devices.
 */
class MultipleDevices final : PortListener {
  std::array<DeviceDescriptor *, NUMDEV> devices;
  std::array<DeviceDispatcher *, NUMDEV> dispatchers;

  DeviceBlackboard &blackboard;

  Mutex listeners_mutex;
  std::list<PortListener *> listeners;

  /**
   * A replay is running and sends its flight to the "NMEA output"
   * ports: they carry the replayed sentences only, and the sentences
   * received from the other ports are not passed on to them (see
   * DeviceDispatcher).
   */
  std::atomic_bool replay_active{false};

public:
  MultipleDevices(DeviceBlackboard &blackboard,
                  NMEALogger *nmea_logger,
                  DeviceFactory &factory) noexcept;
  ~MultipleDevices() noexcept;

  DeviceDescriptor &operator[](unsigned i) const noexcept {
    return *devices[i];
  }

  typedef typename std::array<DeviceDescriptor *, NUMDEV>::const_iterator const_iterator;

  const_iterator begin() noexcept {
    return devices.begin();
  }

  const_iterator end() noexcept {
    return devices.end();
  }

  /**
   * Invoke Device::OnSysTicker() on all devices.
   */
  void Tick() noexcept;

  void Open(OperationEnvironment &env) noexcept;
  void Close() noexcept;
  void AutoReopen(OperationEnvironment &env) noexcept;

  [[gnu::pure]]
  bool HasVega() const noexcept;

#ifdef HAVE_REMOTE_STICK
  /**
   * True if the fixed SteFly slot (REMOTE_PORT or ROTARY_PORT) has
   * been set up at startup because its device is attached.  Controls
   * whether the DeviceListDialog shows the row at all.
   */
  [[gnu::pure]]
  bool HasSteFlyDevice(unsigned slot) const noexcept;
#endif

  /**
   * Start or end a replay that sends its flight to the "NMEA output"
   * ports: while it runs, the sentences received from the devices are
   * no longer passed on to these ports, so that a device behind such a
   * port gets the replayed flight alone and not mixed with the live
   * data.
   */
  void SetReplayActive(bool active) noexcept {
    replay_active.store(active, std::memory_order_relaxed);
  }

  [[gnu::pure]]
  bool IsReplayActive() const noexcept {
    return replay_active.load(std::memory_order_relaxed);
  }

  /**
   * Send a replayed sentence to all "NMEA output" ports.
   */
  void ForwardReplayLine(const char *line) noexcept;

  void VegaWriteNMEA(const char *text, OperationEnvironment &env) noexcept;

  void PutMacCready(double mac_cready, OperationEnvironment &env) noexcept;
  void PutBugs(double bugs, OperationEnvironment &env) noexcept;
  void PutBallast(double fraction, double overload,
                  OperationEnvironment &env) noexcept;
  void PutCrewMass(double crew_mass, OperationEnvironment &env) noexcept;
  void PutEmptyMass(double empty_mass, OperationEnvironment &env) noexcept;
  void PutMaxTakeoffMass(double max_takeoff_mass,
                         OperationEnvironment &env) noexcept;
  void PutPolar(const GlidePolar &polar, OperationEnvironment &env) noexcept;
  void PutTarget(const GeoPoint &location, const char *name,
                 std::optional<double> elevation,
                 OperationEnvironment &env) noexcept;
  void PutVolume(unsigned volume, OperationEnvironment &env) noexcept;
  void PutPilotEvent(OperationEnvironment &env) noexcept;
  void PutActiveFrequency(RadioFrequency frequency, const char *name,
                          OperationEnvironment &env) noexcept;
  void PutStandbyFrequency(RadioFrequency frequency, const char *name,
                           OperationEnvironment &env) noexcept;
  void ExchangeRadioFrequencies(OperationEnvironment &env) noexcept;
  void PutTransponderCode(TransponderCode code, OperationEnvironment &env) noexcept;
  void PutQNH(AtmosphericPressure pres, OperationEnvironment &env) noexcept;
  void PutElevation(int elevation, OperationEnvironment &env) noexcept;
  void RequestElevation(OperationEnvironment &env) noexcept;
  void NotifySensorUpdate(const MoreData &basic) noexcept;
  void NotifyCalculatedUpdate(const MoreData &basic,
                              const DerivedInfo &calculated) noexcept;

  void AddPortListener(PortListener &listener) noexcept;
  void RemovePortListener(PortListener &listener) noexcept;

private:
  /* virtual methods from class PortListener */
  void PortStateChanged() noexcept override;
  void PortError(const char *msg) noexcept override;
};
