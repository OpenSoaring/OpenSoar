// SPDX-License-Identifier: BSD-2-Clause
// Copyright OpenSoar
// author: Uwe Augustin <Info@OpenSoar.de>

#include "Get.hpp"
#include "File.hpp"
#include "system/FileUtil.hpp"
#include "LocalPath.hpp"

#include <boost/json.hpp>

#include <string>
#include <fstream>

namespace Json {
  static boost::json::value system_config;

  struct _structJsonConfig {
    enumConfigs type;
    Path path;
    boost::json::value json_value;
  };
  _structJsonConfig structJsonConfig[enumConfigs::LAST];
  // static boost::json::value json_configs[enumConfigs::LAST];

  boost::json::value &Load(enumConfigs config, const Path &path) {
    if (File::Exists(path)) {
      int buf_size = File::GetSize(path) + 1;
      char *buffer = new char[buf_size]; // = 4k
      try {
        if (File::ReadString(path, buffer, buf_size)) {
          // system_config = boost::json::parse(buffer);
          structJsonConfig[config].type = config;
          structJsonConfig[config].path = path;
          structJsonConfig[config].json_value = boost::json::parse(buffer);
        }
      }
      catch ([[maybe_unused]] std::exception &e) {
      }
      delete[] buffer;
    } else {
      // file not exists, create empty json object
      structJsonConfig[config].type = config;
      structJsonConfig[config].path = path;
      structJsonConfig[config].json_value = boost::json::object{};
    }
    return structJsonConfig[config].json_value;
  }

  bool
  Save(enumConfigs config)
  {
    if (structJsonConfig[config].path.empty()) {
      structJsonConfig[config].type = config;
      structJsonConfig[config].path = GetCachePath();
      // structJsonConfig[config].json_value = boost::json::object{};
    }
    std::fstream os(structJsonConfig[config].path.c_str(),
      std::fstream::out | std::fstream::binary);
    if (os.is_open() == false) {
      return false;
    } else {
      PrettyPrint(os, structJsonConfig[config].json_value, 2);
      os.close();
      return true;
    }
  }

  void
  PrettyPrint(std::ostream &os, boost::json::value const &jv,
      const size_t indent_size, std::string *indent)
  {
    std::string indent_;
    if (!indent)
      indent = &indent_;
    switch (jv.kind())
    {
      case boost::json::kind::object:
      {
        os << "{" << std::endl;
        indent->append(indent_size, ' ');
        auto const &obj = jv.get_object();
        if (!obj.empty())
        {
          auto it = obj.begin();
          for (;;)
          {
            os << *indent << "\"" << it->key().data() << "\" : ";
            PrettyPrint(os, it->value(), indent_size, indent);
            if (++it == obj.end())
              break;
            os << "," << std::endl;
          }
        }
        os << std::endl;
        indent->resize(indent->size() - indent_size);
        os << *indent << "}";
        break;
      }

      case boost::json::kind::array:
      {
        os << "[" << std::endl;
        indent->append(indent_size, ' ');
        auto const &arr = jv.get_array();
        if (!arr.empty())
        {
          auto it = arr.begin();
          for (;;)
          {
            os << *indent;
            PrettyPrint(os, *it, indent_size, indent);
            if (++it == arr.end())
              break;
            os << "," << std::endl;
          }
        }
        os << std::endl;
        indent->resize(indent->size() - indent_size);
        os << *indent << "]";
        break;
      }

      case boost::json::kind::string:
      {
        os << "\"" << jv.get_string().data() << "\"";
        break;
      }

      case boost::json::kind::uint64:
        os << jv.get_uint64();
        break;

      case boost::json::kind::int64:
        os << jv.get_int64();
        break;

      case boost::json::kind::double_:
        os << jv.get_double();
        break;

      case boost::json::kind::bool_:
        if (jv.get_bool())
          os << "true";
        else
          os << "false";
        break;

      case boost::json::kind::null:
        os << "null";
        break;
    }

    if (indent->empty())
      os << std::endl;
  }


  boost::json::value &
    GetValue(enumConfigs config)
  {
    return structJsonConfig[config].json_value;
  }
  boost::json::object &
    GetObject(enumConfigs config)
  {
    return structJsonConfig[config].json_value.as_object();
  }

} // namespace Json
