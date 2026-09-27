// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Geo.hpp"
#include "ui/opengl/System.hpp"
#include "Projection/WindowProjection.hpp"
#include "Geo/GeoPoint.hpp"
#include "Geo/WebMercator.hpp"

#include <glm/gtc/matrix_transform.hpp>

glm::mat4
ToGLM(const WindowProjection &projection, const GeoPoint &reference) noexcept
{
  auto angle = projection.GetScreenAngle().Radians();
  const PixelPoint &screen_origin = projection.GetScreenOrigin();
  const GeoPoint &screen_location = projection.GetGeoLocation();

  /* the vertices are Mercator coordinates relative to the reference
     point (see ImportShapePoint() in Topography/XShape.cpp), so a
     linear transformation is exact: no per-vertex trigonometry is
     needed to follow the map projection */
  const auto delta_x = (reference.longitude - screen_location.longitude)
    .AsDelta().Radians();
  const auto delta_y = WebMercator::LatitudeToY(reference.latitude)
    - WebMercator::LatitudeToY(screen_location.latitude);

  const auto scale = projection.GetMercatorScale();
  const auto scale_x = scale;
  const auto scale_y = -scale;

  const glm::vec3 scale_vec(GLfloat(scale_x), GLfloat(scale_y), 1);

  glm::mat4 matrix = glm::scale(glm::rotate(glm::translate(glm::mat4(1),
                                                           glm::vec3(screen_origin.x,
                                                                     screen_origin.y,
                                                                     0)),
                                            GLfloat(angle),
                                            glm::vec3(0, 0, -1)),
                                scale_vec);
  matrix = glm::translate(matrix,
                          glm::vec3(GLfloat(delta_x),
                                    GLfloat(delta_y),
                                    0.));
  return matrix;
}
