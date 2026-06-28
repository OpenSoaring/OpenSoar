// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Device/Driver.hpp"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"

#include <chrono>
#include <string_view>

class Port;
class OperationEnvironment;

/**
 * Base class for drivers that expose an async, block-oriented
 * settings-and-info exchange:
 *
 *   - the driver issues a "query" sentence (e.g. "$POPSQ,Settings*XX"),
 *   - the device responds with zero or more "field" sentences
 *     (e.g. "$PSRCS,R,Layout=2*XX"),
 *   - the device terminates the response with a "Ready" sentence
 *     (e.g. "$PSRCS,R,Ready*XX"), which unblocks the caller.
 *
 * Each block has its own #PendingBlock (Mutex + Cond + ready flag), so
 * the Info channel and the Settings channel are independent and can be
 * waited on in parallel without blocking each other.
 *
 * Two concrete consumers today: BlueFly Vario (legacy multi-sentence
 * BFV/BST/SET) and SteFly RemoteStick (PSRCI/PSRCS with explicit
 * "Ready" terminator). Future drivers with the same async R/W pattern
 * (XCVario settings, AR62xx, …) can drop in trivially.
 *
 * @note This class does NOT replace AbstractDevice's polymorphic
 *       ParseNMEA / LinkTimeout. Subclasses still override those.
 *       What it provides is a small reusable toolbox for the
 *       send-query / wait-for-block / write-setting choreography.
 */
class ManagedDevice : public AbstractDevice {
public:
  /**
   * Synchronisation state for one block-oriented response.
   */
  struct PendingBlock {
    Mutex mutex;
    Cond cond;
    /**
     * Set to true once the terminating "Ready" sentence has been parsed,
     * or once a full block of fields has been parsed (driver-specific).
     */
    bool ready = false;

    /** Reset the ready flag, typically called right before sending the query. */
    void Reset() noexcept {
      const std::lock_guard lock{mutex};
      ready = false;
    }

    /** Mark the block as ready and wake everyone waiting in WaitFor(). */
    void NotifyReady() noexcept {
      {
        const std::lock_guard lock{mutex};
        ready = true;
      }
      cond.notify_all();
    }

    /**
     * Block until #ready becomes true or the timeout expires.
     * @return true if the block completed, false on timeout.
     */
    bool WaitFor(std::chrono::milliseconds timeout) noexcept {
      std::unique_lock lock{mutex};
      if (!ready)
        cond.wait_for(lock, timeout);
      return ready;
    }
  };

protected:
  Port &port;

  explicit ManagedDevice(Port &_port) noexcept :port(_port) {}

  /**
   * Build "$<sentence>*XX" and write it to the port. Wraps
   * PortWriteNMEA() — sentence is the body between '$' and '*'.
   */
  void SendNMEA(const char *sentence, OperationEnvironment &env);

  /**
   * Convenience for "single value per setting" protocols.
   *
   * Builds "<write_talker>,<name>,<value>" and sends it.
   * Example for the SteFly stick:
   *   WriteDeviceSetting("POPSQ", "Layout", "2", env)
   *   -> "$POPSQ,Layout,2*XX"
   *
   * Subclasses can also call SendNMEA directly if their protocol
   * uses a different separator or no name/value structure.
   */
  void WriteDeviceSetting(const char *write_talker,
                          const char *name,
                          std::string_view value,
                          OperationEnvironment &env);
};
