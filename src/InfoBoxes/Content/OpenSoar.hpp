// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

/**
 * The contents of the OpenSoar InfoBox block (see
 * InfoBoxes/Content/Extension.hpp).  Plain update functions; the
 * table in Extension.cpp wraps them into InfoBoxContent objects.
 */

struct InfoBoxData;

/* wind */
void UpdateInfoBoxInstantaneousWindSpeed(InfoBoxData &data) noexcept;
void UpdateInfoBoxInstantaneousWindBearing(InfoBoxData &data) noexcept;
void UpdateInfoBoxInternalWind(InfoBoxData &data) noexcept;
void UpdateInfoBoxInternalZigZagWind(InfoBoxData &data) noexcept;

/* settings and switches */
void UpdateInfoBoxSTFSwitch(InfoBoxData &data) noexcept;
void UpdateInfoBoxBugsSetting(InfoBoxData &data) noexcept;
void UpdateInfoBoxTrueHeading(InfoBoxData &data) noexcept;
void UpdateInfoBoxWaterBallast(InfoBoxData &data) noexcept;

/* system */
void UpdateInfoBoxPageNo(InfoBoxData &data) noexcept;
