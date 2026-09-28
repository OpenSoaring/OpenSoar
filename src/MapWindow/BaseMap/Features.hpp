// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

/* raster tile base maps are drawn as OpenGL textures (see
   MapOverlayBitmap) and are fetched over HTTP */
#if defined(ENABLE_OPENGL) && defined(HAVE_HTTP)
#define HAVE_BASE_MAP_TILES
#endif
