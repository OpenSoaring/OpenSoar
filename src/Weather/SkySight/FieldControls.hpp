// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Weather/MapOverlay/FieldPickerResult.hpp"
#include "util/StaticString.hxx"

struct PageLayout;

namespace SkySight {

struct Layer;

using LayerPickerResult = WeatherMapOverlay::FieldPickerResult;

void ApplyCursorFromPageLayout(const PageLayout &page) noexcept;

[[nodiscard]]
bool EditTimeOnLayout(PageLayout &page) noexcept;

void OpenTimePicker() noexcept;

[[nodiscard]]
LayerPickerResult EditLayerOnLayout(PageLayout &page,
                                    bool offer_setup=false) noexcept;

[[nodiscard]]
LayerPickerResult OpenLayerPicker(bool offer_setup=false) noexcept;

void FormatTimeLabelForPage(StaticString<64> &text,
                            const PageLayout &page) noexcept;

/**
 * The time row of a live layer: "Live (hh:mm)" with the time of the
 * images on the map, or of the newest images known when none is shown
 * yet; "updating" adds that newer tiles are being downloaded.
 */
void FormatLiveTimeLabel(StaticString<64> &text, const Layer &layer,
                         bool updating) noexcept;

void FormatLayerLabelForPage(StaticString<64> &text,
                             const PageLayout &page) noexcept;

[[nodiscard]] bool IsTimeSelectable(const PageLayout &page) noexcept;
[[nodiscard]] bool IsTimeSelectable() noexcept;
[[nodiscard]] bool HasSelectedLayer() noexcept;
[[nodiscard]] bool HasSelectedTimeData() noexcept;

[[nodiscard]] bool StepTime(int delta) noexcept;
[[nodiscard]] bool StepLayer(int delta) noexcept;

[[nodiscard]] bool GetTimeAutoAdvance() noexcept;
void SetTimeAutoAdvance(bool auto_advance) noexcept;
void ApplyAutoAdvanceTime() noexcept;
void EnableTimeAutoFromInput() noexcept;

} // namespace SkySight
