// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

// aug2 10.03.2024: #if defined(RC_FILE)
// aug2 10.03.2024: #  define MAKE_RESOURCE(name, id) const int name = id
// aug2 10.03.2024: #else
#include "ResourceId.hpp"

#ifdef USE_WIN32_RESOURCES

#define MAKE_RESOURCE(name, file, id) \
  static constexpr ResourceId name(id);

#elif defined(ANDROID)

#define MAKE_RESOURCE(name, file, id) \
  static constexpr ResourceId name{#file};

#else

// aug 10.03.2024: #endif
// aug2 10.03.2024: #endif   // RC_FILE
#include <cstddef>

#define MAKE_RESOURCE(name, file, id) \
  extern "C" std::byte resource_ ## name[]; \
  extern "C" const size_t resource_ ## name ## _size; \
  static constexpr ResourceId name(resource_ ## name, &resource_ ## name ## _size);

#endif

// aug 10.03.2024: 
#include "MakeResource.hpp"


#undef MAKE_RESOURCE
