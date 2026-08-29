// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

/*
 * A repository is a hand-maintained text file on somebody's web
 * server: the entry can be wrong, and then a download either brings
 * the wrong kind of file or a "not found" page.  These checks make
 * sure the program notices - and, just as important, that it does not
 * complain about files that are perfectly fine.
 */

#include "Repository/FileCheck.hpp"
#include "TestUtil.hpp"
#include "io/FileOutputStream.hxx"
#include "system/FileUtil.hpp"
#include "system/Path.hpp"
#include "util/SpanCast.hxx"

#include <string>
#include <string_view>

#include <stdlib.h>

static AllocatedPath temp_dir;

static AllocatedPath
WriteFile(const char *name, std::string_view content)
{
  auto path = AllocatedPath::Build(temp_dir, Path{name});
  FileOutputStream out(path, FileOutputStream::Mode::CREATE);
  out.Write(AsBytes(content));
  out.Commit();
  return path;
}

static void
TestGoodFiles()
{
  /* a SeeYou waypoint file */
  const auto cup = WriteFile("wp.cup",
                             "name,code,country,lat,lon,elev,style,rwdir\n"
                             "\"Aachen\",AAC,DE,5050.500N,00606.000E,200.0m,2,\n");
  ok1(CheckFileContent(cup, FileType::WAYPOINT) == FileCheckResult::OK);

  /* a .cupx: a ZIP container */
  const auto cupx = WriteFile("wp.cupx",
                              std::string_view{"PK\x03\x04....binary....", 20});
  ok1(CheckFileContent(cupx, FileType::WAYPOINT) == FileCheckResult::OK);

  /* a map file is a ZIP as well */
  const auto xcm = WriteFile("map.xcm",
                             std::string_view{"PK\x03\x04\x14\x00\x00", 7});
  ok1(CheckFileContent(xcm, FileType::MAP) == FileCheckResult::OK);

  /* OpenAir airspace */
  const auto air = WriteFile("as.txt",
                             "* Aachen\nAC D\nAN EDKA CTR\nAL GND\nAH 2500ft\n"
                             "DP 50:50:30 N 006:06:00 E\n");
  ok1(CheckFileContent(air, FileType::AIRSPACE) == FileCheckResult::OK);

  /* an IGC flight log */
  const auto igc = WriteFile("flight.igc",
                             "AXCS000\nHFDTE290826\nB1101355016925N00606080EA\n");
  ok1(CheckFileContent(igc, FileType::IGC) == FileCheckResult::OK);

  /* an XML task */
  const auto tsk = WriteFile("task.tsk",
                             "<?xml version=\"1.0\"?>\n<Task type=\"RT\">\n");
  ok1(CheckFileContent(tsk, FileType::TASK) == FileCheckResult::OK);

  /* the types without a reliable signature stay quiet */
  const auto rasp = WriteFile("x-rasp.dat", std::string_view{"\x01\x02\x00\x03", 4});
  ok1(CheckFileContent(rasp, FileType::RASP) == FileCheckResult::UNKNOWN);
}

static void
TestNotFoundPage()
{
  /* the most common accident: the server answers with a web page */
  const auto html = WriteFile("wp2.cup",
                              "<!DOCTYPE html>\n<html><head><title>404 Not "
                              "Found</title></head><body>nope</body></html>\n");
  ok1(CheckFileContent(html, FileType::WAYPOINT) == FileCheckResult::HTML);

  /* with a BOM and leading whitespace in front of it */
  const auto html2 = WriteFile("as2.txt",
                               "\xef\xbb\xbf  \n<HTML><body>Error</body></HTML>\n");
  ok1(CheckFileContent(html2, FileType::AIRSPACE) == FileCheckResult::HTML);

  /* and it is wrong for every type, not just for text ones */
  const auto html3 = WriteFile("map2.xcm", "<html>404</html>\n");
  ok1(CheckFileContent(html3, FileType::MAP) == FileCheckResult::HTML);
}

static void
TestWrongType()
{
  /* a waypoint file where a map was expected */
  const auto cup = WriteFile("wrong.xcm",
                             "name,code,country,lat,lon,elev,style\n");
  ok1(CheckFileContent(cup, FileType::MAP) == FileCheckResult::MISMATCH);

  /* a map (ZIP) where airspace was expected */
  const auto zip = WriteFile("wrong.txt",
                             std::string_view{"PK\x03\x04\x00\x00", 6});
  ok1(CheckFileContent(zip, FileType::AIRSPACE) == FileCheckResult::MISMATCH);

  /* prose instead of airspace */
  const auto prose = WriteFile("readme.txt",
                               "This file explains how to use the airspace "
                               "data.\nPlease read carefully.\n");
  ok1(CheckFileContent(prose, FileType::AIRSPACE) == FileCheckResult::MISMATCH);

  /* an empty file */
  const auto empty = WriteFile("empty.cup", "");
  ok1(CheckFileContent(empty, FileType::WAYPOINT) == FileCheckResult::EMPTY);

  /* a file that is not there at all */
  ok1(CheckFileContent(Path{"/nonexistent/file.cup"}, FileType::WAYPOINT) ==
      FileCheckResult::UNREADABLE);
}

static void
TestMessages()
{
  ok1(GetFileCheckMessage(FileCheckResult::OK) == nullptr);
  ok1(GetFileCheckMessage(FileCheckResult::UNKNOWN) == nullptr);
  ok1(GetFileCheckMessage(FileCheckResult::HTML) != nullptr);
  ok1(GetFileCheckMessage(FileCheckResult::MISMATCH) != nullptr);
  ok1(GetFileCheckMessage(FileCheckResult::EMPTY) != nullptr);
}

/**
 * The name in the repository must carry an extension the type
 * accepts - the mistake that started all this: "westalpen_de (cupx)"
 * for a waypoint file.
 */
static void
TestFileName()
{
  ok1(CheckFileName("westalpen_de.cupx", FileType::WAYPOINT));
  ok1(CheckFileName("westalpen_de.cup", FileType::WAYPOINT));
  ok1(!CheckFileName("westalpen_de (cupx)", FileType::WAYPOINT));
  ok1(!CheckFileName("westalpen_de", FileType::WAYPOINT));
  ok1(!CheckFileName("westalpen_de.xcm", FileType::WAYPOINT));

  ok1(CheckFileName("alps.xcm", FileType::MAP));
  ok1(!CheckFileName("alps.cup", FileType::MAP));

  /* without a type there is nothing to check against */
  ok1(CheckFileName("anything at all", FileType::UNKNOWN));
  ok1(!CheckFileName("", FileType::WAYPOINT));
  ok1(!CheckFileName(nullptr, FileType::WAYPOINT));
}

int
main()
{
  char template_path[] = "/tmp/opensoar-filecheck-XXXXXX";
  if (mkdtemp(template_path) == nullptr)
    return 1;

  temp_dir = AllocatedPath{Path{template_path}};

  plan_tests(30);

  TestGoodFiles();
  TestNotFoundPage();
  TestWrongType();
  TestMessages();
  TestFileName();

  return exit_status();
}
