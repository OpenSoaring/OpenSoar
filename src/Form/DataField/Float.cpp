// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Float.hpp"
#include "ComboList.hpp"
#include "Math/Util.hpp"
#include "util/NumberParser.hpp"

#include <stdio.h>

static bool DataFieldKeyUp = false;

const char *
DataFieldFloat::GetAsString() const noexcept
{
  snprintf(mOutBuf, sizeof(mOutBuf), edit_format, (double)mValue);
  return mOutBuf;
}

const char *
DataFieldFloat::GetAsDisplayString() const noexcept
{
  snprintf(mOutBuf, sizeof(mOutBuf), display_format, (double)mValue, unit.c_str());
  return mOutBuf;
}

void
DataFieldFloat::ModifyValue(double Value) noexcept
{
  if (Value < mMin)
    Value = mMin;
  if (Value > mMax)
    Value = mMax;
  if (Value != GetValue()) {
    SetValue(Value);
    Modified();
  }
}

void
DataFieldFloat::Inc() noexcept
{
  // no keypad, allow user to scroll small values
  if (mFine && mValue < 0.95 && mStep >= 0.5 &&
      mMin >= 0)
    ModifyValue(mValue + 0.1);
  else
    ModifyValue(mValue + mStep * SpeedUp(true));
}

void
DataFieldFloat::Dec() noexcept
{
  // no keypad, allow user to scroll small values
  if (mFine && mValue <= 1 && mStep >= 0.5 &&
      mMin >= 0)
    ModifyValue(mValue - 0.1);
  else
    ModifyValue(mValue - mStep * SpeedUp(false));
}

double
DataFieldFloat::SpeedUp(bool keyup) noexcept
{
  if (keyup != DataFieldKeyUp) {
    mSpeedup = 0;
    DataFieldKeyUp = keyup;
    last_step.Update();
    return 1;
  }

  if (!last_step.Check(std::chrono::milliseconds(200))) {
    mSpeedup++;
    if (mSpeedup > 5) {
      last_step.UpdateWithOffset(std::chrono::milliseconds(350));
      return 10;
    }
  } else
    mSpeedup = 0;

  last_step.Update();

  return 1;
}

void
DataFieldFloat::SetFromCombo([[maybe_unused]] int iDataFieldIndex, const char *sValue) noexcept
{
  ModifyValue(ParseDouble(sValue));
}

void
DataFieldFloat::AppendComboValue(ComboList &combo_list,
                                 double value) const noexcept
{
  char a[decltype(edit_format)::capacity()], b[decltype(display_format)::capacity()];
  snprintf(a, sizeof(a), edit_format, (double)value);
  snprintf(b, sizeof(b), display_format, (double)value, unit.c_str());
  combo_list.Append(a, b);
}

ComboList
DataFieldFloat::CreateComboList(const char *reference_string) const noexcept
{
  const auto reference = reference_string != nullptr
    ? ParseDouble(reference_string)
    : mValue;

  ComboList combo_list;
  const auto epsilon = mStep / 1000;
  const auto fine_step = mStep / 10;

  /* how many items before and after the current value? */
  unsigned surrounding_items = ComboList::MAX_SIZE / 2 - 2;
  if (mFine)
    surrounding_items -= 20;

  /* the value aligned to mStep */
  auto corrected_value = int((reference - mMin) / mStep) * mStep + mMin;

  auto first = corrected_value - surrounding_items * mStep;
  if (first > mMin + epsilon)
    /* there are values before "first" - give the user a choice */
    combo_list.Append(ComboList::Item::PREVIOUS_PAGE, "<<More Items>>");
  else if (first < mMin - epsilon)
    first = int(mMin / mStep) * mStep;

  auto last = std::min(first + surrounding_items * mStep * 2, mMax);

  bool found_current = false;
  auto step = mStep;
  bool inFineSteps = false;
  for (auto i = first; i <= last + epsilon; i += step) {

    // Skip over the items which fall below the beginning of the valid range.
    // e.g. first may be 0.0 for values with valid range 0.1 - 10.0 and step 1.0
    // rather than duplicate all the fine_step setup above simply ignore the few
    // values here. Needed for nice sequence e.g. 1.0 2.0 ... instead of 1.1 2.1 ...
    if (i < mMin - epsilon)
      continue;

    if (mFine) {
      // show up to 9 items above and below current value with extended precision
      if (i - epsilon > reference + mStep - fine_step) {
        if (inFineSteps) {
          inFineSteps = false;
          step = mStep;
          i = int((i + mStep - fine_step) / mStep) * mStep;
          if (i > mMax)
            i = mMax;
        }
      }
      else if (i + epsilon >= reference - mStep + fine_step) {
        if (!inFineSteps) {
          inFineSteps = true;
          step = fine_step;
          i = std::max(mMin, reference - mStep + fine_step);
        }
      }
    }

    if (!found_current && reference <= i + epsilon) {
      combo_list.current_index = combo_list.size();

      if (reference < i - epsilon)
        /* the current value is not listed - insert it here */
        AppendComboValue(combo_list, reference);

      found_current = true;
    }

    AppendComboValue(combo_list, i);
  }

  if (reference > last + epsilon) {
    /* the current value out of range - append it here */
    last = reference;
    combo_list.current_index = combo_list.size();
    AppendComboValue(combo_list, reference);
  }

  if (last < mMax - epsilon)
    /* there are values after "last" - give the user a choice */
    combo_list.Append(ComboList::Item::NEXT_PAGE, "<<More Items>>");

  return combo_list;
}
