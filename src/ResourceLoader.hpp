// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <span>

#ifdef _WIN32
#include <windef.h>
#endif


class ResourceId;

namespace ResourceLoader {

#ifdef _WIN32
bool Initialized();

void
Init(HINSTANCE hInstance);
#endif

using Data = std::span<const std::byte>;

Data
Load(const char *name, const char *type);

#ifndef ANDROID
Data
Load(ResourceId id);
#endif

#ifdef USE_WIN32_RESOURCES
HBITMAP
LoadResBitmap(ResourceId id);
#endif

} // namespace ResourceLoader
