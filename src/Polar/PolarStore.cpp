// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Polar/PolarStore.hpp"
#include "Polar/Polar.hpp"
#include "Units/System.hpp"

#include <cassert>

namespace PolarStore {

PolarShape
Item::ToPolarShape() const noexcept
{
  PolarShape shape;

  shape[0].v = Units::ToSysUnit(v1, Unit::KILOMETER_PER_HOUR);
  shape[0].w = w1;
  shape[1].v = Units::ToSysUnit(v2, Unit::KILOMETER_PER_HOUR);
  shape[1].w = w2;
  shape[2].v = Units::ToSysUnit(v3, Unit::KILOMETER_PER_HOUR);
  shape[2].w = w3;
  shape.reference_mass = reference_mass;

  return shape;
}

PolarInfo
Item::ToPolarInfo() const noexcept
{
  PolarInfo polar;

  polar.max_ballast = max_ballast;
  polar.shape = ToPolarShape();
  polar.wing_area = wing_area;
  polar.v_no = v_no;

  return polar;
}

static constexpr Item default_polar = {
  "LS-8 (15m)", 325, 185, 70, -0.51, 115, -0.85, 173, -2.00, 10.5, 0.0, 108, 240,
};

/**
 *  Note: Please keep in alphabetic order to ease finding and updateing.
 *        Index to entries are not used as reference in profiles. Data is copied
 *        as initial values only, because the table will be target of further refinement
 *        for ever.
 */
static constexpr Item internal_polars[] = {
  { "206 Hornet", 318, 100, 80, -0.606, 120, -0.99, 160, -1.918, 9.8, 41.666, 100, 227 },
  { "303 Mosquito", 450, 0, 100.0, -0.68, 120.0, -0.92, 150.0, -1.45, 9.85, 0.0, 107, 242 },
  { "304CZ", 310, 115, 115.03, -0.86, 174.04, -1.76, 212.72, -3.4, 0, 0.0, 110, 235 },
  { "401 Kestrel (17m)", 367, 33, 95, -0.62, 110, -0.76, 175, -2.01, 11.58 , 0.0, 110, 260 },
  { "604 Kestrel", 570, 100, 112.97, -0.72, 150.64, -1.42, 207.13, -4.1, 16.26, 0.0, 114, 455 },

  // from Akaflieg Karlsruhe, idaflieg measurement 2017 with new winglet design at Aalen Elchingen 
  { "AK-8", 362 , 100, 84.1343, -0.6524, 130.0, -0.9474, 170.0, -1.8380, 9.75, 50.0, 107, 233 },

  // from LX8000/9000 simulator
  { "Antares 18S", 350, 250, 100, -0.54, 120, -0.63, 150, -1.07, 10.97, 0.0, 120, 295 },
  { "Antares 18T", 395, 205, 100, -0.54, 120, -0.69, 150, -1.11, 10.97, 0.0, 120, 345 },
  { "Antares 20E", 530, 130, 100, -0.52, 120, -0.61, 150, -0.91, 12.6, 0.0, 123, 475 },

  { "Apis (13m)", 200, 45, 100, -0.74, 120, -1.01, 150, -1.66, 10.36, 0.0, 93, 137 },
  { "Apis 2 (15m)", 310, 0, 80, -0.60, 100, -0.75, 140, -1.45, 12.40, 0.0, 98, 215 }, // from LK8000

  // from Shempp Hirth
  { "Arcus", 700, 185, 110, -0.64, 140, -0.88, 180, -1.47, 15.59, 50.0, 120, 430 },

  { "ASG-29 (15m)", 362, 165, 108.8, -0.635, 156.4, -1.182, 211.13, -2.540, 9.20, 0.0, 114, 270 },
  { "ASG-29 (18m)", 355, 225, 85, -0.47, 90, -0.48, 185, -2.00, 10.5, 0.0, 121, 280 },
  { "ASG-29E (15m)", 350, 200, 100, -0.64, 120, -0.75, 150, -1.13, 9.2, 0.0, 114, 315 },
  { "ASG-29E (18m)", 400, 200, 90, -0.499, 95.5, -0.510, 196.4, -2.12, 10.5, 0.0, 121, 325 },
  { "ASH-25", 750, 121, 130.01, -0.78, 169.96, -1.4, 219.94, -2.6, 16.31, 0.0, 122, 478 },
  { "ASH-26", 340, 185, 100, -0.56, 120, -0.74, 150, -1.16, 11.7, 0.0, 119, 325 },
  { "ASH-26E", 435, 90, 90, -0.51, 96, -0.53, 185, -2.00, 11.7, 0.0, 119, 360 },
  { "ASK-13", 456, 0, 85, -0.84, 120, -1.5, 150, -2.8, 17.5, 44.444, 79, 296 },
  { "ASK-18", 310, 0, 75, -0.613, 138, -1.773, 200, -4.234, 12.99, 0, 88, 215 },
  { "ASK-21", 468, 0, 74.1, -0.67, 101.9, -0.90, 166.7, -2.68, 17.95, 50.0, 92, 360 },
  { "ASK-23", 330, 0, 100, -0.85, 120, -1.19, 150, -2.02, 12.9, 0.0, 92, 240 },
  { "ASW-12", 394, 189, 95, -0.57, 148, -1.48, 183.09, -2.6, 13.00, 0.0, 110, 324 },
  { "ASW-15", 349, 91, 97.56, -0.77, 156.12, -1.9, 195.15, -3.4, 11.0, 0.0, 97, 210 },
  { "ASW-17", 522, 151, 114.5, -0.7, 169.05, -1.68, 206.5, -2.9, 14.84, 0.0, 115, 405 },
  { "ASW-19", 363, 125, 97.47, -0.74, 155.96, -1.64, 194.96, -3.1, 11.0, 47.222, 100, 240 },
  { "ASW-20", 377, 159, 116.2, -0.77, 174.3, -1.89, 213.04, -3.3, 10.5, 0.0, 108, 255 },
  { "ASW-20BL", 400, 126, 95, -0.628, 148, -1.338, 200, -2.774, 10.49, 0, 112, 275 },
  { "ASW-22B", 597, 303, 80, -0.4015, 120, -0.66, 160, -1.3539, 16.31, 0.0, 123, 270 },
  { "ASW-22BLE", 465, 285, 100, -0.47, 120, -0.63, 150, -1.04, 16.7, 0.0, 124, 275 },
  { "ASW-24", 350, 159, 108.82, -0.73, 142.25, -1.21, 167.41, -1.8, 10.0, 0.0, 107, 230 },
  { "ASW-27", 365, 165, 88.8335, -0.5939, 130.0, -0.8511, 170.0, -1.6104, 9.0, 0.0, 114, 235 }, // from idaflieg 1997 & 2018
  { "ASW-28 (15m)", 310, 200, 92.6, -0.571, 120.38, -0.875, 148.16, -1.394, 10.5, 55.555, 108, 235 }, // from SeeYou
  { "ASW-28 (18m)", 345, 190, 65, -0.47, 107, -0.67, 165, -2.00, 10.5, 0.0, 114, 270 },
  { "Blanik L13", 472, 0, 85.0, -0.84, 143.0, -3.32, 200.0, -9.61, 19.1, 0.0, 78, 292 },
  // from factory polar (flight manual)
  { "Blanik L13-AC", 500, 0, 70, -0.85, 110, -1.25, 160, -3.2, 17.44, 44.44, 78, 306 },
  { "Blanik L23", 510, 0, 95.0, -0.94, 148.0, -2.60, 200.0, -6.37, 19.1, 0.0, 80, 310 },
  { "Carat", 470, 0, 100, -0.83, 120, -1.04, 150, -1.69, 10.58, 0.0, 93, 341 },
  { "Cirrus (18m)", 330, 100, 100, -0.74, 120, -1.06, 150, -1.88, 12.6, 0.0, 102, 260 },

  // Idaflieg measurement, 28.08.2015 at Aalen Elchingen
  { "D-43 18m", 668, 0, 100, -0.62, 130, -0.863, 170, -1.672, 15.93, 250, 99,  },

  { "Delta USHPA-2", 100, 0, 30, -1.10, 44.3,  -1.52,  58.0, -3.60,  0, 0.0, 0, 5 },
  { "Delta USHPA-3", 100, 0, 37, -0.95, 48.1,  -1.15,  73.0, -3.60,  0, 0.0, 0, 5 },
  { "Delta USHPA-4", 100, 0, 37, -0.89, 48.3,  -1.02,  76.5, -3.30,  0, 0.0, 0, 5 },
  { "DG-1000 (20m)", 613, 160, 106.0, -0.62, 153.0, -1.53, 200.0, -3.2, 17.51, 0.0, 111, 415 },
  { "DG-100", 300, 100, 100, -0.73, 120, -1.00, 150, -1.7, 11.0, 0.0, 100, 230 },
  { "DG-200", 300, 120, 100, -0.68, 120, -0.86, 150, -1.3, 10.0, 0.0, 107, 230 },
  { "DG-300", 310, 190, 95.0, -0.66, 140.0, -1.28, 160.0, -1.70, 10.27, 0.0, 104, 235 },
  { "DG-400 (15m)", 440, 90, 115, -0.76, 160.53, -1.22, 210.22, -2.3, 10.0, 0.0, 107, 306 },
  { "DG-400 (17m)", 444, 90, 118.28, -0.68, 163.77, -1.15, 198.35, -1.8, 10.57, 0.0, 109, 310 },
  { "DG-500 (20m)", 659, 100, 115.4, -0.71, 152.01, -1.28, 190.02, -2.3, 18.29, 0.0, 104, 390 },
  { "DG-600 (15m)", 327, 180, 100, -0.60, 120, -0.76, 150, -1.19, 10.95, 0.0, 110, 255 },
  { "DG-800B (15m)", 468, 100, 103.610, -0.6535, 130.0, -0.8915, 170.0, -1.4807, 10.68, 52.8, 113, 340 },
  { "DG-800B (18m)", 472, 100, 90.0086, -0.5496, 130.0, -0.7920, 170.0, -1.4248, 11.81, 52.8, 119, 344 },
  // as taken from handbook of DG-800S the pure glider variant
  { "DG-800S (15m)", 370, 150, 92.1255, -0.5811, 130.0, -0.9754, 170.0, -1.6933, 10.68, 52.8, 113, 260 },
  { "DG-800S (18m)", 350, 150, 77.5081, -0.4733, 130.0, -0.9257, 170.0, -1.7948, 11.81, 52.8, 119, 264 },

  { "Dimona", 670, 100, 100, -1.29, 120, -1.61, 150, -2.45, 15.3, 0.0, 68, 470 },
  { "Discus 2b", 312, 200, 105, -0.66, 150, -1.05, 200, -2.0, 10.6, 0.0, 108, 252 },
  { "Discus 2c (18m)", 377, 188, 100, -0.57, 120, -0.76, 150, -1.33, 11.36, 55.555, 114, 280 },
  { "Discus", 350, 182, 95, -0.63, 140, -1.23, 180, -2.29, 10.58, 55.555, 107, 233 },
  { "Duo Discus", 615, 80, 103, -0.64, 152, -1.25, 200, -2.51, 16.4, 0.0, 112, 420 },
  { "Duo Discus T", 615, 80, 103, -0.64, 152, -1.25, 200, -2.51, 16.4, 0.0, 112, 445 },
  { "Duo Discus xT", 700, 50, 110, -0.664, 155, -1.206, 200, -2.287, 16.40, 50.0, 113, 445 },
  { "EB 28", 670, 180, 100, -0.46, 120, -0.61, 150, -0.96, 16.8, 0.0, 125, 570 },
  { "EB 28 Edition", 670, 180, 100, -0.47, 120, -0.63, 150, -0.97, 16.5, 0.0, 125, 570 },
  { "G 102 Astir CS", 330, 90, 75.0, -0.7, 93.0, -0.74, 185.00, -3.1, 12.40, 0.0, 96, 255 },

  // from factory polar (flight manual) by Christopher Schenk
  { "G 102 Club Astir IIIb", 380, 0, 75.0, -0.6, 100.0, -0.70, 180.00, -3.1, 12.40, 0.0, 91, 255 },
  { "G 102 Standard Astir III", 380, 70, 75.0, -0.6, 100.0, -0.70, 180.00, -2.8, 12.40, 0.0, 100, 260 },

  { "G 103 Twin 2", 580, 0, 99, -0.8, 175.01, -1.95, 225.02, -3.8, 17.52, 0.0, 92, 390 },
  { "G 104 Speed Astir", 351, 90, 90, -0.63, 105, -0.72, 157, -2.00, 11.5, 0.0, 105, 265 },
  { "Genesis II", 374, 151, 94, -0.61, 141.05, -1.18, 172.4, -2.0, 11.24, 0.0, 107, 240 },
  { "Glasfluegel 304", 305, 145, 100, -0.78, 120, -0.97, 150, -1.43, 9.9, 0.0, 110, 235 },
  { "H-201 Std Libelle", 304, 50, 97, -0.79, 152.43, -1.91, 190.54, -3.3, 9.8, 41.666, 98, 185 },
  { "H-205 Club Libelle", 295, 0, 100, -0.85, 120, -1.21, 150, -2.01, 9.8, 41.666, 96, 200 },
  { "H-301 Libelle", 300, 50, 94, -0.68, 147.71, -2.03, 184.64, -4.1, 9.8, 41.666, 100, 180 },
  { "IS-28B2", 590, 0, 100, -0.82, 160, -2.28, 200, -4.27, 18.24, 0.0, 84, 375 },
  { "IS-29D2 Lark", 360, 0, 100, -0.82, 135.67, -1.55, 184.12, -3.3, 10.4, 0.0, 96, 240 },
  { "Janus (18m)", 498, 240, 100, -0.71, 120, -0.92, 150, -1.46, 16.6, 0.0, 102, 405 },
  { "Janus C FG", 603, 170, 115.5, -0.76, 171.79, -1.98, 209.96, -4.0, 17.4, 47.222, 106, 405 }, // obviously wrong
  { "Janus C RG", 519, 240, 90, -0.60, 120, -0.88, 160, -1.64, 17.3, 50, 108, 405 }, // from factory polar
  { "JS-1B (18m)", 405, 180, 108, -0.57, 152, -1.06, 180, -1.65, 11.25, 56.3, 121, 310 }, // from factory polar
  { "JS-1C (21m)", 441, 180, 108, -0.52, 156, -1.1, 180, -1.62, 12.25, 56.3, 126, 330 }, // from factory polar
  { "JS-3 (15m)", 350, 158, 100, -0.6, 130, -0.8, 160, -1.2, 8.75, 57.5, 116, 270 }, // from factory polar
  { "JS-3 (18m)", 398, 158, 100, -0.55, 130, -0.72, 160, -1.12, 9.95, 57.5, 122, 282 }, // from factory polar
  { "Ka 2b", 418, 0, 87, -0.9, 120, -1.5, 150, -2.6, 17.5, 0.0, 78, 278 },
  { "Ka 4 Rhoenlerche", 360, 0, 65, -0.95, 120, -2.5, 140, -3.5, 16.34, 0.0, 54, 220 },
  { "Ka 6CR", 310, 0, 64.83 , -0.67 , 130.0, -2.26 , 170.0, -4.69 , 12.5 , 0.0 , 82, 185 },
  { "Ka 6E",  310, 0, 87.35, -0.81, 141.92, -2.03, 174.68, -3.5, 12.4, 0.0, 85, 185 }, // from idaflieg measured 1970
  { "Ka 7", 445, 0, 87, -0.92, 120, -1.55, 150, -2.7, 17.5, 0.0, 78, 285 },
  { "Ka 8", 290, 0, 74.1, -0.76, 101.9, -1.27, 166.7, -4.64, 14.15, 0.0, 76, 190 },
  { "L 33 Solo", 330, 0, 87.2, -0.8, 135.64, -1.73, 174.4, -3.4, 11.0, 0.0, 86, 210 },
  { "LAK-12",  430, 190, 75, -0.48, 125, -0.88, 175, -1.97, 14.63, 48.611, 114, 360 }, // from marius@sargevicius.com
  { "LAK-17 (15m)", 285, 215, 100, -0.60, 120, -0.72, 150, -1.09, 9.06, 0.0, 113, 220 },
  { "LAK-17 (18m)", 295, 205, 100, -0.56, 120, -0.74, 150, -1.16, 9.8, 0.0, 119, 225 },
  { "LAK17a (15m)", 285, 180, 95, -0.574, 148, -1.310, 200, -2.885, 9.06, 0.0, 113, 220 },
  { "LAK17a (18m)", 298, 180, 115, -0.680, 158, -1.379, 200, -2.975, 9.80, 0.0, 119, 225 },
  { "LAK-19 (15m)", 285, 195, 100, -0.64, 120, -0.85, 150, -1.41, 9.06, 0.0, 108, 220 },
  { "LAK-19 (18m)", 295, 185, 100, -0.60, 120, -0.82, 150, -1.34, 9.8, 0.0, 114, 226 },
  { "LS-10s (15m)", 370, 170, 100, -0.64, 120, -0.80, 150, -1.26, 10.27, 0.0, 113, 288 },
  { "LS-10s (18m)", 380, 220, 100, -0.58, 120, -0.75, 150, -1.21, 11.45, 0.0, 119, 295 },
  { "LS-1c", 350, 91, 115.87, -1.02, 154.49, -1.84, 193.12, -3.3, 9.74, 0.0, 98, 200 },
  { "LS-1f", 345, 80, 100, -0.75, 120, -0.98, 150, -1.6, 9.74, 0.0, 100, 230 }, // from idaflieg
  { "LS-3 (17m)", 325, 0, 100, -0.61, 120, -0.84, 150, -1.53, 11.22, 0.0, 109, 255 },
  { "LS-3", 383, 121, 93.0, -0.64, 127.0, -0.93, 148.2, -1.28, 10.5, 0.0, 107, 270 },
  { "LS-4", 361, 121, 100, -0.69, 120, -0.87, 150, -1.44, 10.5, 0.0, 104, 235 },
  // Derived from Idaflieg measurements, D-7742, measured at 13-15/8/1991 in Aalen. Empty mass from comments by the owner here: https://www.youtube.com/watch?v=wSX3k-Lp7HA.
  { "LS-5", 461, 120, 75, -0.45, 135, -1.0, 172.5, -1.9, 13.9, 0.0, 118, 361 },
  { "LS-6 (15m)", 327, 160, 90, -0.6, 100, -0.658, 183, -1.965, 10.53, 0.0, 111, 265 },
  { "LS-6 (18m)", 330, 140, 90, -0.51, 100, -0.57, 183, -2.00, 11.4, 0.0, 117, 272 },
  { "LS-7wl", 350, 150, 103.77, -0.73, 155.65, -1.47, 180.00, -2.66, 9.80, 0.0, 107, 235 },
  { "LS-8 (15m)", 325, 185, 70, -0.51, 115, -0.85, 173, -2.00, 10.5, 0.0, 108, 240 },
  { "LS-8 (18m)", 325, 185, 80, -0.51, 94, -0.56, 173, -2.00, 11.4, 0.0, 114, 250 },
  { "Mini Nimbus", 345, 155, 100, -0.69, 120, -0.92, 150, -1.45, 9.86, 55.555, 107, 235 },
  { "Nimbus 2", 493, 159, 119.83, -0.75, 179.75, -2.14, 219.69, -3.8, 14.41, 44.444, 114, 350 },
  { "Nimbus 3", 527, 159, 116.18, -0.67, 174.28, -1.81, 232.37, -3.8, 16.70, 52.777, 122, 396 },
  { "Nimbus 3DM", 820, 168, 114.97, -0.57, 157.42, -0.98, 222.24, -2.3, 16.70, 52.777, 121, 595 },
  { "Nimbus 3D", 712, 168, 93.64, -0.46, 175.42, -1.48, 218.69, -2.5, 16.70, 52.777, 121, 485 },
  { "Nimbus 3T", 577, 310, 141.7, -0.99, 182.35, -1.89, 243.13, -4.0, 16.70, 52.777, 121, 422 },
  { "Nimbus 4", 597, 303, 85.1, -0.41, 127.98, -0.75, 162.74, -1.4, 17.8, 50.0, 124, 470 },
  { "Nimbus 4DM", 820, 168, 100.01, -0.48, 150.01, -0.87, 190.76, -1.6, 17.8, 50.0, 123, 595 },
  { "Nimbus 4D", 743, 303, 107.5, -0.5, 142.74, -0.83, 181.51, -1.6, 17.8, 50.0, 123, 515 },
  { "Para Competition", 100, 0, 39.0, -1.0, 45.0,  -1.1, 64.0, -2.30,  0, 0.0, 0 },
  { "Para EN A/DHV1", 100, 0, 29.0, -1.1,  34.0, -1.3, 44.0, -2.30, 0, 0.0, 0 },
  { "Para EN B/DHV12", 100, 0, 29.5, -1.1, 37.0, -1.2, 50.0, -2.30, 0, 0.0, 0 },
  { "Para EN C/DHV2", 110, 4.19, 33.0, -1.0, 39.0, -1.2, 56.0, -2.30, 0, 0.0, 0 },
  { "Para EN D/DHV23", 100, 0, 33.0, -1.1, 41.0, -1.2, 58.0, -2.30,  0, 0.0, 0 },
  { "Pegase 101A", 344, 120, 85.0, -0.62, 105, -0.75, 175, -2.54, 10.5, 0.0, 102, 252 },
  { "Phoebus C", 310, 150, 100, -0.70, 120, -0.98, 150, -1.58, 14.06, 0.0, 100, 225 },
  { "PIK-20B", 354, 144, 102.5, -0.69, 157.76, -1.59, 216.91, -3.6, 10.0, 0.0, 102, 240 },
  { "PIK-20D", 348, 144, 100, -0.69, 156.54, -1.78, 215.24, -4.2, 10.0, 0.0, 104, 227 },
  { "PIK-20E", 437, 80, 109.61, -0.83, 166.68, -2, 241.15, -4.7, 10.0, 0.0, 104, 324 },
  { "PIK-30M", 460, 0, 123.6, -0.78, 152.04, -1.12, 200.22, -2.2, 10.63, 0.0, 0, 347 },
  // Derived from Pilatus B4 PH-448, fixed gear serial production, measured at Idaflieg-vergleichsfliegen at Aalen in the year 1973.
  { "Pilatus B4 FG", 306, 0, 90.0, -0.847, 126.0, -1.644, 198.0, -5.098, 14.0, 0.0, 86, 230 },
  { "PW-5 Smyk", 300, 0, 99.5, -0.95, 158.48, -2.85, 198.1, -5.1, 10.16, 0.0, 85, 190 },
  { "PW-6", 546, 0, 104, -0.847, 152, -1.994, 200, -4.648, 15.3, 0, 86, 360 },
  { "R-26S Gobe", 420, 0, 60.0, -1.02, 80.0, -0.96, 120.0, -2.11, 18.00, 30.5, 0, 230 },
  { "Russia AC-4", 250, 0, 99.3, -0.92, 140.01, -1.8, 170.01, -2.9, 7.70, 0.0, 84, 140 },
  { "SF-27B", 300, 0, 100, -0.81, 120, -1.27, 150, -2.50, 13, 0.0, 88, 220 },
  { "SGS 1-26E", 315, 0, 82.3, -1.04, 117.73, -1.88, 156.86, -3.8, 14.87, 0.0, 63, 202 },
  { "SGS 1-34", 354, 0, 89.82, -0.8, 143.71, -2.1, 179.64, -3.8, 14.03, 0.0, 85, 259 },
  { "SGS 1-35A", 381, 179, 98.68, -0.74, 151.82, -1.8, 202.87, -3.9, 9.64, 0.0, 0, 180 },
  { "SGS 1-36 Sprite", 322, 0, 75.98, -0.68, 132.96, -2, 170.95, -4.1, 13.10, 0.0, 76, 215 },
  { "SGS 2-33", 472, 0, 71.53, -0.96, 112.97, -1.74, 147.73, -3.44, 20.35, 33, 54, 272 },
   // From Silene E78/E78B manual, available on Issoire Aviation website
  { "Silene E78", 450, 0, 75, -0.548, 125, -1.267, 160, -2.439, 18, 47.222, 0, 365 },
  { "Skylark 4", 395, 0, 78, -0.637, 139, -2, 200, -5.092, 16.1, 0, 84, 258 },
  { "Std Austria S", 297, 0, 70.0, -0.70, 118.0, -1.0, 145.0, -1.5, 13.5 , 38.9, 90, 205}, // From Schempp-Hirth sales handout, potentially too good
  { "Std Cirrus", 337, 80, 93.23, -0.74, 149.17, -1.71, 205.1, -4.2, 10.04, 0.0, 99, 215 },
  { "Stemme S-10", 850, 0, 133.47, -0.83, 167.75, -1.41, 205.03, -2.3, 18.70, 0.0, 110, 645 },
  { "SZD-30 Pirat", 370, 0, 80, -0.72, 100, -0.98, 150, -2.46, 13.8, 0.0, 86, 260 },
  { "SZD-36 Cobra", 350, 30, 70.8, -0.60, 94.5, -0.69, 148.1, -1.83, 11.6, 0.0, 98, 275 },
  { "SZD-42 Jantar II", 482, 191, 109.5, -0.66, 157.14, -1.47, 196.42, -2.7, 14.27, 0.0, 113, 362 },
  { "SZD-48-2 Jantar Std 2", 375, 150, 100, -0.73, 120, -0.95, 150, -1.60, 10.66, 0.0, 100, 274 },
  { "SZD-48-3 Jantar Std 3", 326, 150, 95, -0.66, 180, -2.24, 220, -3.85, 10.66, 0.0, 100, 274 },
  { "SZD-50 Puchacz", 435, 135, 100, -1.00, 120, -1.42, 150, -2.35, 18.16, 0.0, 84, 370 },
  { "SZD-51-1 Junior", 333, 0, 70, -0.58, 130, -1.6, 180, -3.6, 12.51, 0.0, 90, 242 },

  // From Perkoz handbook
  { "SZD-54-2 Perkoz (FT 17m)" /* flat tip */,     442, 0, 98, -0.92, 174, -4.35, 250, -13.22, 16.36, 47.05, 98, 375 },
  { "SZD-54-2 Perkoz (WL 17m)" /* winglet */,      442, 0, 99, -0.86, 175, -4.22, 250, -13.01, 16.36, 47.05, 98, 375 },
  { "SZD-54-2 Perkoz (WL 20m)" /* long winglet */, 442, 0, 91, -0.69, 170, -3.98, 250, -12.66, 17.30, 47.05, 102, 380 },

  { "SZD-55-1 Promyk", 350, 200, 100.0, -0.66, 120, -0.86, 150, -1.4, 9.60, 0.0, 106, 215 },
  { "SZD-9 bis 1E Bocian", 540, 0, 70, -0.83, 90, -1.00, 140, -2.53, 20, 0.0, 76, 330 },
  { "Taurus", 472, 0, 100, -0.71, 120, -0.83, 150, -1.35, 12.26, 0.0, 99, 306 },
  { "Ventus 2c (18m)", 385, 180, 80.0, -0.5, 120.0, -0.73, 180.0, -2.0, 11.03, 55.555, 120, 260 },
  { "Ventus 2cT (18m)", 410, 110, 100.0, -0.62, 150.0, -1.2, 200.0, -2.3, 11.03, 55.555, 120, 305 },
  { "Ventus 2cx (18m)", 385, 215, 80.0, -0.5, 120.0, -0.73, 180.0, -2.0, 11.03, 55.555, 120, 265 },
  { "Ventus 2cxT (18m)", 470, 130, 100.0, -0.56, 150.0, -1.13, 200.0, -2.28, 11.03, 55.555, 120, 310 },
  { "Ventus a/b (16.6m)", 358, 151, 100.17, -0.64, 159.69, -1.47, 239.54, -4.3, 9.96, 55.555, 113, 250 },
  { "Ventus b (15m)", 341, 151, 97.69, -0.68, 156.3, -1.46, 234.45, -3.9, 9.51, 55.555, 110, 240 },
  { "Ventus cM (17.6)", 430, 0, 100.17, -0.6, 159.7, -1.32, 210.54, -2.5, 10.14, 55.555, 115, 360 },
  // from LK8000
  { "VSO-10 Gradient", 347, 0, 90, -0.78, 130, -1.41, 160, -2.44, 12.0, 44.444, 96, 250 },
  { "VT-116 Orlik II", 335, 0, 80, -0.7, 100, -1.05, 120, -1.65, 12.8, 33.333, 86, 215 },

  // from factory polar.
  // flight manual http://www.issoire-aviation.fr/doc_avia_gen/MdV_WA26P_R2.pdf
  // Contest handicap reference: http://docplayer.fr/79733029-Handicaps-planeurs-ffvv.html
  { "WA 26 P Squale",  330, 0, 80, -0.61, 152, -2, 174, -3.0, 12.6, 0.0, 86, 228 },

  { "Zuni II", 358, 182, 110, -0.88, 167, -2.21, 203.72, -3.6, 10.13, 0.0, 0, 238 },
};

const Item &
GetDefault() noexcept
{
  return default_polar;
}

std::span<const Item>
GetAll() noexcept
{
  return internal_polars;
}

} // namespace PolarStore
