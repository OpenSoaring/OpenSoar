// SPDX-License-Identifier: BSD-2-Clause
// Copyright OpenSoar
// author: Uwe Augustin <Info@OpenSoar.de>

#include "Get.hpp"
#include "LogFile.hpp"

namespace Json {
  static boost::json::value json_null;

  boost::json::value &GetNull() {
    return json_null;
  }

  boost::json::value &
    GetValue(boost::json::value &root, std::vector<std::string_view> args) noexcept
  {
    boost::json::value *value = &root;
    // std::string name = value->try_as_string()-> .c_str();  // "sys_config";
    std::string name = value->if_string() ? value->get_string().c_str() : "[]";
    size_t i = 0;
    try {
      for (auto str : args) {
        i++;
        if (value->is_object() && value->as_object().if_contains(str)) {
          value = &value->at(str);
          name += "->";
          name += str;
        } else {
          LogFmt("Json::GetValue: Param {}: {}->{} not exists!", i, name, str);
          return json_null;
        }
      }
      return *value;
    }
    catch ([[maybe_unused]]const std::exception &e) {
      LogFmt("Json-Exception GetConfigBool({}): {}", args.size(), e.what());
    }
    return json_null;
  }

  boost::json::value &
    GetValue(boost::json::value &root, std::string_view str) noexcept
  {
    std::vector<std::string_view> args;
    for (auto x = str.find('.'); x < 1000; x = str.find('.')) {
      args.push_back(str.substr(0, x));
      str = str.substr(x + 1);

    }
    args.push_back(str);
    return GetValue(root, args);
  }

  boost::json::value &
    GetValue(enumConfigs config, std::string_view str) noexcept
  {
    return GetValue(GetValue(config), str);
  }

  boost::json::value &
    GetValue(enumConfigs config, std::vector<std::string_view> args) noexcept
  {
    return  GetValue(GetValue(config), args);
  }

  bool
    GetBool(boost::json::value &root, std::vector<std::string_view> args) noexcept
  {
    auto value = GetValue(root, args);
    if (value.is_bool())
      return value.as_bool();
    else {
      return false;
    }
  }

  bool
    GetBool(boost::json::value &root, std::string_view str) noexcept
  {
    auto value = GetValue(root, str);
    if (value.is_bool())
      return value.as_bool();
    else {
      return false;
    }
  }

  boost::json::object &
    GetObject(boost::json::value &root, std::string_view str) noexcept
  {
    auto value = GetValue(root, str);
    if (value.is_object())
      return value.as_object();
    else {
      return value.as_object();   // last valid value
    }
  }

  boost::json::object &
  GetObject(enumConfigs config, std::string_view str) noexcept
  {
    auto value = GetValue(config, str);
    if (value.is_object())
      return value.as_object();
    else {
      return value.as_object();   // last valid value
    }
  }

} // namespace Json
