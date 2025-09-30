// SPDX-License-Identifier: BSD-2-Clause
// Copyright OpenSoar
// author: Uwe Augustin <Info@OpenSoar.de>

#pragma once

#include "File.hpp"

#include <boost/json.hpp>

namespace Json {
  boost::json::value &GetNull();

  boost::json::value &GetValue(boost::json::value &root, std::vector<std::string_view> args) noexcept;
  boost::json::value &GetValue(boost::json::value &root, std::string_view str) noexcept;

  boost::json::value &GetValue(enumConfigs config, std::vector<std::string_view> args) noexcept;
  boost::json::value &GetValue(enumConfigs config, std::string_view str) noexcept;

  boost::json::object &GetObject(boost::json::value &root, std::string_view str) noexcept;
  boost::json::object &GetObject(enumConfigs config, std::string_view str) noexcept;

  bool GetBool(boost::json::value &root, std::vector<std::string_view> args) noexcept;
  bool GetBool(boost::json::value &root, std::string_view str) noexcept;

};// namespace Json
