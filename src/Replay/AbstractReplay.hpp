// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

struct NMEAInfo;

class AbstractReplay 
{
public:
  virtual ~AbstractReplay() {}

  virtual bool Update(NMEAInfo &data) = 0;

  /**
   * How much of the input has been consumed?  Returns 0..1, or a
   * negative value when unknown.
   */
  virtual double GetProgress() const noexcept {
    return -1;
  }
};
