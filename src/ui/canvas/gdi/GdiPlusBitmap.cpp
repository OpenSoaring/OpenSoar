// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "GdiPlusBitmap.hpp"
#include "UTF8Win.hpp"

# include <algorithm>
using std::min;  // to avoid the missing 'min' in the gdiplush headers
using std::max;  // to avoid the missing 'max' in the gdiplush headers

#include "UTF8Win.hpp"

#ifdef GDI_WITH_TESTSAVE
#include "system/Path.hpp"
#include "LocalPath.hpp"

# include <fstream>
# include <iosfwd>
#endif // GDI_WITH_TESTSAVE

#include <assert.h>
#include <unknwn.h>
#include <gdiplus.h>

static ULONG_PTR gdiplusToken;

//----------------------------------------------------------------------------
void
GdiStartup()
{
  Gdiplus::GdiplusStartupInput gdiplusStartupInput;
  Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
}

//----------------------------------------------------------------------------
void
GdiShutdown()
{
  Gdiplus::GdiplusShutdown(gdiplusToken);
}

//----------------------------------------------------------------------------
// can load: BMP, GIF, JPEG, PNG, TIFF, Exif, WMF, and EMF
HBITMAP
GdiLoadImage(std::string_view filename)
{
  HBITMAP result = nullptr;
  // filename has to be WCHAR* in GdiPlus
  Gdiplus::Bitmap bitmap(UTF8ToWide(filename).c_str(), false);
  if (bitmap.GetLastStatus() != Gdiplus::Ok)
    return nullptr;
  const Gdiplus::Color color = Gdiplus::Color::White;
  if (bitmap.GetHBITMAP(color, &result) != Gdiplus::Ok)
    return nullptr;
  return result;
}

HBITMAP
GdiLoadImage(UncompressedImage &&uncompressed)
{
  HBITMAP result = nullptr;
  BITMAPINFO bmi;
  memset(&bmi, 0, sizeof(bmi));
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = uncompressed.GetWidth();
  bmi.bmiHeader.biHeight = uncompressed.GetHeight();
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;  // BI_BITFIELDS;
  bmi.bmiHeader.biSizeImage =
      4 * bmi.bmiHeader.biWidth * bmi.bmiHeader.biHeight;

  BITMAPFILEHEADER bmfh;
  int nBitsOffset = sizeof(BITMAPFILEHEADER) + bmi.bmiHeader.biSize;
  bmfh.bfType = 'B' + ('M' << 8);
  bmfh.bfOffBits = nBitsOffset;
  bmfh.bfSize = nBitsOffset + bmi.bmiHeader.biSizeImage;
  bmfh.bfReserved1 = bmfh.bfReserved2 = 0;

  Gdiplus::Bitmap bitmap( &bmi, const_cast<void *> (uncompressed.GetData()));

  if (bitmap.GetLastStatus() != Gdiplus::Ok) return nullptr;
  const Gdiplus::Color color = Gdiplus::Color::White;
  if (bitmap.GetHBITMAP(color, &result)  // Black or White?
    != Gdiplus::Ok) return nullptr;
  return result;
}