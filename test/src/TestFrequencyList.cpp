// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

/*
 * The frequency list is a hand-written file; these checks make sure
 * both formats are read, that a typo hides only its own line, and
 * that the format is told apart by content rather than by name.
 */

#include "FrequencyList.hpp"
#include "TestUtil.hpp"
#include "io/FileOutputStream.hxx"
#include "system/Path.hpp"
#include "util/SpanCast.hxx"

#include <string_view>

#include <stdlib.h>

static bool
Is(const RadioStation &s, const char *name, unsigned khz,
   const char *comment = "")
{
  return s.name == name && s.frequency.IsDefined() &&
    s.frequency.GetKiloHertz() == khz && s.comment == comment;
}

static void
TestText()
{
  const auto list = ParseFrequencyList(
    "# Wettbewerb 2026\r\n"
    "\r\n"
    "Stuttgart Info : 128.950\r\n"
    "Start          : 129.890 # Startleitung\n"
    "Cloud=130.535 Wolkenflug\n"
    "Tower\t118.800\n"
    "Komma : 122,800\n"
    "kaputt : 12.3\n"
    "ohne Trenner 123.450\n"
    "  : 122.800\n"
    "Leer :\n");

  ok1(list.size() == 5);
  ok1(Is(list[0], "Stuttgart Info", 128950));
  ok1(Is(list[1], "Start", 129890, "Startleitung"));
  ok1(Is(list[2], "Cloud", 130535, "Wolkenflug"));
  ok1(Is(list[3], "Tower", 118800));
  ok1(Is(list[4], "Komma", 122800));
}

static void
TestJson()
{
  const auto list = ParseFrequencyList(
    "{ \"stations\": [\n"
    "  { \"name\": \"Stuttgart Info\", \"frequency\": \"128.950\","
    "    \"comment\": \"FIS\" },\n"
    "  { \"name\": \"Start\", \"frequency\": 129.890 },\n"
    "  { \"name\": \"kaputt\", \"frequency\": \"abc\" },\n"
    "  { \"frequency\": \"122.800\" },\n"
    "  { \"name\": \"Tower\", \"frequency\": 118.8 }\n"
    "] }\n");

  ok1(list.size() == 3);
  ok1(Is(list[0], "Stuttgart Info", 128950, "FIS"));
  ok1(Is(list[1], "Start", 129890));
  ok1(Is(list[2], "Tower", 118800));

  /* a bare array works as well */
  const auto bare = ParseFrequencyList(
    "[ { \"name\": \"A\", \"frequency\": \"122.800\" } ]");
  ok1(bare.size() == 1);
  ok1(Is(bare[0], "A", 122800));

  /* with a BOM and leading whitespace */
  const auto bom = ParseFrequencyList(
    "\xef\xbb\xbf \n { \"stations\": [ { \"name\": \"B\", \"frequency\": \"122.800\" } ] }");
  ok1(bom.size() == 1);

  /* broken JSON: nothing, but no crash */
  ok1(ParseFrequencyList("{ \"stations\": [ { \"name\": ").empty());
  ok1(ParseFrequencyList("{ \"foo\": 1 }").empty());
}

static void
TestEmpty()
{
  ok1(ParseFrequencyList("").empty());
  ok1(ParseFrequencyList("# only a comment\n").empty());
  ok1(ParseFrequencyList("\n\n\n").empty());
}

static void
TestFile()
{
  char template_path[] = "/tmp/opensoar-freq-XXXXXX";
  if (mkdtemp(template_path) == nullptr) {
    ok1(false);
    ok1(false);
    ok1(false);
    return;
  }

  const auto path = AllocatedPath::Build(Path{template_path},
                                         Path{"wettbewerb.xcf"});
  {
    FileOutputStream out(path, FileOutputStream::Mode::CREATE);
    out.Write(AsBytes(std::string_view{"Start : 129.890\n"}));
    out.Commit();
  }

  const auto list = LoadFrequencyList(path);
  ok1(list.size() == 1);
  ok1(Is(list[0], "Start", 129890));

  ok1(LoadFrequencyList(Path{"/nonexistent/x.xcf"}).empty());
}

int
main()
{
  plan_tests(21);

  TestText();
  TestJson();
  TestEmpty();
  TestFile();

  return exit_status();
}
