// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Widget.hpp"

#include <cassert>
#include <memory>

class ButtonPanel;

/**
 * Wrap a #ButtonPanel around another #Widget instance.
 */
class ButtonPanelWidget : public Widget {
public:
  enum class Alignment {
    AUTO, BOTTOM, LEFT
  };

private:
  const std::unique_ptr<Widget> widget;
  std::unique_ptr<ButtonPanel> buttons;
  Alignment alignment;

  /** Landscape gives this row to the child. */
  bool hide_in_landscape = false;

  /**
   * With #Alignment::BOTTOM: the buttons before this index form an
   * upper row, the others a lower one (see
   * ButtonPanel::TwoRowBottomLayout()).  0 means one row.
   */
  unsigned bottom_row_split = 0;

public:
  ButtonPanelWidget(std::unique_ptr<Widget> &&_widget,
                    Alignment _alignment=Alignment::AUTO) noexcept
    :widget(std::move(_widget)), alignment(_alignment) {}

  void HideInLandscape() noexcept {
    hide_in_landscape = true;
  }

  /**
   * Lay out the buttons at the bottom in two rows, the first \a split
   * buttons above the others, for two groups that belong apart.  Call
   * before the widget is prepared.
   */
  void SetBottomRowSplit(unsigned split) noexcept {
    bottom_row_split = split;
  }

  ~ButtonPanelWidget() noexcept override;

  Widget &GetWidget() noexcept {
    return *widget;
  }

  ButtonPanel &GetButtonPanel() noexcept {
    assert(buttons != nullptr);

    return *buttons;
  }

private:
  unsigned GetButtonRows() const noexcept {
    return alignment == Alignment::BOTTOM && bottom_row_split > 0 ? 2 : 1;
  }

  PixelRect UpdateLayout(const PixelRect &rc) noexcept;

public:
  PixelSize GetMinimumSize() const noexcept override;
  PixelSize GetMaximumSize() const noexcept override;
  void Initialise(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  void Unprepare() noexcept override;
  bool Save(bool &changed) noexcept override;
  bool Click() noexcept override;
  void ReClick() noexcept override;
  void Show(const PixelRect &rc) noexcept override;
  bool Leave() noexcept override;
  void Hide() noexcept override;
  void Move(const PixelRect &rc) noexcept override;
  bool SetFocus() noexcept override;
  bool HasFocus() const noexcept override;
  bool KeyPress(unsigned key_code) noexcept override;
};
