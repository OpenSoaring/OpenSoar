// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "util/NonCopyable.hpp"

#include <forward_list>

class Canvas;
class WindowProjection;
class LabelBlock;
class TopographyStore;
class TopographyFileRenderer;
struct TopographyLook;

/**
 * Class used to manage and render vector topography layers
 */
class TopographyRenderer : private NonCopyable {
  const TopographyStore &store;

  std::forward_list<TopographyFileRenderer> files;

public:
  TopographyRenderer(const TopographyStore &store,
                     const TopographyLook &look) noexcept;

  TopographyRenderer(const TopographyRenderer &) = delete;

  ~TopographyRenderer() noexcept;

  const TopographyStore &GetStore() const noexcept {
    return store;
  }

  /**
   * Draws the topography to the given canvas
   * @param canvas The drawing canvas
   * @param rc The area to draw in
   */
  void Draw(Canvas &canvas, const WindowProjection &projection) noexcept;

  /**
   * Like Draw(), but with the given map scale deciding which layers
   * are visible and how much they are thinned, instead of the one of
   * @a projection.  Used to draw into a buffer larger than the
   * screen, whose larger size would otherwise hide layers that are
   * visible on the screen.
   */
  void Draw(Canvas &canvas, const WindowProjection &projection,
            double map_scale) noexcept;

  void DrawLabels(Canvas &canvas, const WindowProjection &projection,
                  LabelBlock &label_block) noexcept;
};
