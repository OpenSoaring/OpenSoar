// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Base.hpp"
#include "util/StaticString.hxx"

static constexpr unsigned EDITSTRINGSIZE = 32;

class DataFieldString: public DataField
{
  StaticString<EDITSTRINGSIZE> mValue;

protected:
  DataFieldString(Type _type, const char *_value,
                  DataFieldListener *listener=nullptr) noexcept
    :DataField(_type, false, listener), mValue(_value) {}

public:
  DataFieldString(const char *_value,
                  DataFieldListener *listener=nullptr) noexcept
    :DataField(Type::STRING, false, listener), mValue(_value) {}

  const char *GetValue() const noexcept {
    return mValue.c_str();
  }

  void SetValue(const char *new_value) noexcept;
  void ModifyValue(const char *new_value) noexcept;

  /* virtual methods from class DataField */
  const char *GetAsString() const noexcept override;
};
