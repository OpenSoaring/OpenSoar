// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "RowFormWidget.hpp"
#include "Form/Edit.hpp"
#include "Form/DataField/File.hpp"
#include "Form/DataField/Date.hpp"
#include "Profile/Profile.hpp"
#include "LocalPath.hpp"
#include "Formatter/TimeFormatter.hpp"

WndProperty *
RowFormWidget::AddFile(const char *label, const char *help,
                       std::string_view profile_key, const char *filters,
                       FileType file_type,
                       bool nullable) noexcept
{
  WndProperty *edit = Add(label, help);
  auto *df = new FileDataField();
  df->SetFileType(file_type);
  edit->SetDataField(df);

  if (nullable)
    df->AddNull();

  df->ScanMultiplePatterns(filters);

  if (profile_key.data() != nullptr) {
    const auto path = Profile::GetPath(profile_key);
    if (path != nullptr)
      df->SetValue(path);
  }

  edit->RefreshDisplay();

  return edit;
}

WndProperty *
RowFormWidget::AddDirectory(const char *label, const char *help,
                       std::string_view profile_key, bool nullable) noexcept
{
  WndProperty *edit = Add(label, help);
  auto *df = new FileDataField();
  df->SetFileType(FileType::UNKNOWN);
  edit->SetDataField(df);

  if (nullable)
    df->AddNull();

  df->ScanMultiplePatterns("*.*");

  if (profile_key.data() != nullptr) {
    const auto path = Profile::GetPath(profile_key);
    if (path != nullptr)
      df->SetValue(path);
  }

  edit->RefreshDisplay();

  return edit;
}

void
RowFormWidget::SetProfile(std::string_view profile_key, unsigned value) noexcept
{
  Profile::Set(profile_key, value);
}

bool
RowFormWidget::SaveValue(unsigned i, std::string_view profile_key,
                         char *string, size_t max_size) const noexcept
{
  if (!SaveValue(i, string, max_size))
    return false;

  Profile::Set(profile_key, string);
  return true;
}

bool
RowFormWidget::SaveValue(unsigned i, std::string_view profile_key,
                         bool &value, bool negated) const noexcept
{
  if (!SaveValue(i, value, negated))
    return false;

  Profile::Set(profile_key, value);
  return true;
}

bool
RowFormWidget::SaveValue(unsigned i, std::string_view profile_key,
                         double &value) const noexcept
{
  if (!SaveValue(i, value))
    return false;

  Profile::Set(profile_key, value);
  return true;
}

bool
RowFormWidget::SaveValueFileReader(unsigned i,
                                   std::string_view profile_key) noexcept
{
  Path new_value = GetValueFile(i);
  const auto contracted = ContractLocalPath(new_value);
  if (contracted != nullptr)
    new_value = contracted;

  if (!new_value.c_str())
    return false;

  const char *old_value = Profile::Get(profile_key, "");
  if (StringIsEqual(old_value, new_value.c_str()))
    return false;

  Profile::Set(profile_key, new_value.c_str());
  return true;
}

bool
RowFormWidget::SaveValue(unsigned i,
                         std::string_view profile_key,
                         BrokenDate &value) const noexcept
{
  const auto &df = (const DataFieldDate &)GetDataField(i);
  assert(df.GetType() == DataField::Type::DATE);

  const auto new_value = df.GetValue();

  if (!new_value.IsPlausible())
    return false;

  if (new_value == value)
    return false;

  char buffer[0x10];
  FormatISO8601(buffer, new_value);
  Profile::Set(profile_key, buffer);
  value = new_value;
  return true;
}

bool
RowFormWidget::SaveValue(unsigned i,
                         std::string_view profile_key,
                         std::chrono::seconds &value) const noexcept
{
  if (!SaveValue(i, value))
    return false;

  Profile::Set(profile_key, value);
  return true;
}
