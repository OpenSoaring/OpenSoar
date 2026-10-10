// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ui/canvas/Pen.hpp"
#include "ui/canvas/Icon.hpp"
#include "util/Serial.hpp"
#include "Geo/GeoBounds.hpp"
#include "Geo/GeoPoint.hpp"
#include "Math/Angle.hpp"
#include "ui/dim/Size.hpp"

#ifdef ENABLE_OPENGL
#else
#include "ui/canvas/Brush.hpp"
#include "Topography/ShapeRenderer.hpp"
#endif

#include <memory>
#include <vector>

class TopographyFile;
class Canvas;
class GLArrayBuffer;
class WindowProjection;
class LabelBlock;
class XShape;
struct GeoPoint;
struct TopographyLook;

/**
 * Class used to manage and render vector topography layers
 */
class TopographyFileRenderer final
{
  const TopographyFile &file;

  const TopographyLook &look;

#ifndef ENABLE_OPENGL
  mutable ShapeRenderer shape_renderer;
#endif

  Pen pen;

#ifndef ENABLE_OPENGL
  Brush brush;
#endif

  MaskedIcon icon;

  Serial visible_serial;
  GeoBounds visible_bounds;

  std::vector<const XShape *> visible_shapes, visible_labels;

  /**
   * Incremented whenever #visible_labels is rebuilt.
   */
  unsigned visible_generation = 0;

  /**
   * Where each label of #visible_labels goes: the point of its line
   * that is leftmost on the screen.  Finding it projects every point
   * of the shape, which for the lakes and rivers of a large map took
   * a tenth of a second per frame on an OpenVario.  Moving the map
   * without zooming or turning keeps the leftmost point the same, so
   * the search is repeated only when the scale, the screen angle, the
   * label font or the set of visible labels changes.
   */
  struct LabelAnchor {
    const char *label;

    /** the leftmost point, or invalid if no point was found */
    GeoPoint location;

    PixelSize size;
  };

  std::vector<LabelAnchor> label_anchors;
  double anchor_scale = 0;
  Angle anchor_angle = Angle::Zero();
  unsigned anchor_generation = 0;
  int anchor_skip = 0;
  bool anchor_important = false, anchors_valid = false;

  std::vector<GeoPoint> visible_points;

#ifdef ENABLE_OPENGL
  std::unique_ptr<GLArrayBuffer> array_buffer;
  Serial array_buffer_serial;
#endif

public:
  TopographyFileRenderer(const TopographyFile &file,
                         const TopographyLook &look) noexcept;

  TopographyFileRenderer(const TopographyFileRenderer &) = delete;

  ~TopographyFileRenderer() noexcept;

  /**
   * Paints the polygons, lines and points/icons in the TopographyFile
   * @param canvas The canvas to paint on
   * @param bitmap_canvas Temporary canvas for the icon
   * @param projection
   */
  void Paint(Canvas &canvas, const WindowProjection &projection) noexcept;

  /**
   * @param map_scale the scale that decides visibility and thinning,
   * see TopographyRenderer::Draw()
   */
  void Paint(Canvas &canvas, const WindowProjection &projection,
             double map_scale) noexcept;

  /**
   * Paints a topography label if the space is available in the LabelBlock
   * @param canvas The canvas to paint on
   * @param projection
   * @param label_block The LabelBlock class to use for decluttering
   * @param settings_map
   */
  void PaintLabels(Canvas &canvas, const WindowProjection &projection,
                   LabelBlock &label_block) noexcept;

private:
  void UpdateVisibleShapes(const WindowProjection &projection) noexcept;

#ifdef ENABLE_OPENGL
  void UpdateArrayBuffer() noexcept;
#endif

  void PaintPoints(Canvas &canvas, const WindowProjection &projection) noexcept;
};
