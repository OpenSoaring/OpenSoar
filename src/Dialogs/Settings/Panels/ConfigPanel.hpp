// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <functional>

class Button;
class CheckBoxControl;

namespace ConfigPanel {
/**
 * @param i 1-based. Landscape stacks above Previous; portrait shows
 * only 1 and 2 beside Expert.
 */
Button &
BorrowExtraButton(unsigned i, const char *caption,
                  std::function<void()> callback) noexcept;
void ReturnExtraButton(unsigned i);

CheckBoxControl &GetExpertCheckbox() noexcept;

/**
 * Close the configuration dialog and open it again at the current
 * page, once all pages have saved their values.  For a setting that
 * changes the texts of the dialog itself, such as the language, which
 * the pilot wants to see right away.
 */
void RequestReopen() noexcept;
};
