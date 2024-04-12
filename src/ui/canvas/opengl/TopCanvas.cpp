// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ui/canvas/custom/TopCanvas.hpp"
#include "Canvas.hpp"
#include "Globals.hpp"
#include "Init.hpp"
#include "Math/Point2D.hpp"

PixelSize
TopCanvas::GetSize() const noexcept
{
  return {OpenGL::viewport_size.x, OpenGL::viewport_size.y};
}

PixelSize
TopCanvas::SetupViewport(PixelSize native_size) noexcept
{
  auto new_size = OpenGL::SetupViewport(UnsignedPoint2D(native_size.width,
                                                        native_size.height));
  return PixelSize{new_size.x, new_size.y};
}

bool
TopCanvas::CheckResize(PixelSize new_native_size) noexcept
{
  if (new_native_size.width == OpenGL::window_size.x &&
      new_native_size.height == OpenGL::window_size.y)
    return false;

  SetupViewport(new_native_size);
  return true;
}

#ifdef SOFTWARE_ROTATE_DISPLAY

PixelSize
TopCanvas::SetDisplayOrientation(DisplayOrientation orientation) noexcept
{
  OpenGL::display_orientation = orientation;
#if defined (IS_OPENVARIO) 
  // Workaround: ask this one time .. and change it never
  static const PixelSize _size(OpenGL::window_size.x,
                                     OpenGL::window_size.y);
  return SetupViewport(_size);
#else
  return SetupViewport({OpenGL::window_size.x, OpenGL::window_size.y});
#endif
}

#endif

Canvas
TopCanvas::Lock()
{
  return Canvas{GetSize()};
}

void
TopCanvas::Unlock() noexcept
{
}
