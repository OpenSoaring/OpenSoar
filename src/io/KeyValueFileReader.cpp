// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "KeyValueFileReader.hpp"
#include "util/StringCompare.hxx"
#include "LineReader.hpp"

#include <string.h>

static char *
trim(char *text) {
  while (*text == ' ')
    text++;
  char *p = text + strlen(text) - 1;
  while (*p == ' ' && p > text)
    p--;
  *(p + 1) = '\0';
  return text;
}
bool
KeyValueFileReader::Read(KeyValuePair &pair, char split_character)
{
  char *line;
  while ((line = reader.ReadLine()) != nullptr) {
    if (StringIsEmpty(line) || *line == '#')
      continue;

    char *p = strchr(line, split_character);
    if (p == line || p == nullptr)
      continue;

    *p = '\0';
    char *value = p + 1;

    if (*value == '"') {
      ++value;
      p = strchr(value, '"');
      if (p == nullptr)
        continue;

      *p = '\0';
    }
/*
    while (*line == ' ')
      line++;
    p = line + strlen(line) - 1;
    while (*p == ' ' && p > line)
      p--;
    *(p + 1) = '\0';
    while (*value == ' ')
      value++;
    p = value + strlen(value) - 1;
    while (*p == ' ' && p > value)
      p--;
    *(p+1) = '\0';
*/
    pair.key = trim(line);
    pair.value = trim(value);
    return true;
  }

  return false;
}
