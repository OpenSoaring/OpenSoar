// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "GdiPlusBitmap.hpp"
#include "util/UTF8.hpp"

#define GDI_WITH_TESTSAVE
#if defined(_MSC_VER)
# include <algorithm>
using std::min;  // to avoid the missing 'min' in the gdiplush headers
using std::max;  // to avoid the missing 'max' in the gdiplush headers
#endif           // _MSC_VER

#ifdef GDI_WITH_TESTSAVE
# include <fstream>
# include <iosfwd>
#endif // GDI_WITH_TESTSAVE

#include <assert.h>
#include <unknwn.h>
#include <gdiplus.h>

static ULONG_PTR gdiplusToken;

#ifdef GDI_WITH_TESTSAVE
const CLSID bmpEncoder = {0x557cf400,
                          0x1a04,
                          0x11d3,
                          {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
const CLSID jpgEncoder = {0x557cf401,
                          0x1a04,
                          0x11d3,
                          {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
const CLSID gifEncoder = {0x557cf402,
                          0x1a04,
                          0x11d3,
                          {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
const CLSID tifEncoder = {0x557cf405,
                          0x1a04,
                          0x11d3,
                          {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
const CLSID pngEncoder = {0x557cf406,
                          0x1a04,
                          0x11d3,
                          {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
#endif // GDI_WITH_TESTSAVE
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
GdiLoadImage(const char *filename)
{
  HBITMAP result = nullptr;
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

#if defined(GDI_WITH_TESTSAVE) && 0
  std::ofstream file("D:/Data/xxx.bmp", std::ios_base::binary);
  if (!file.is_open())
  {
    return nullptr;
  }
  // Write the bitmap file header
  file.write((const char *)&bmfh, sizeof(BITMAPFILEHEADER));
  UINT nWrittenFileHeaderSize = file.tellp();

  // And then the bitmap info header
  file.write((const char *)&bmi.bmiHeader, sizeof(BITMAPINFOHEADER));
  UINT nWrittenInfoHeaderSize = file.tellp();

  // Finally, write the image data itself
  //-- the data represents our drawing
  file.write((const char *)uncompressed.GetData(), bmi.bmiHeader.biSizeImage);
  UINT nWrittenDIBDataSize = file.tellp();
  file.close();
#endif

  Gdiplus::Bitmap bitmap(&bmi, (void *)uncompressed.GetData());

#ifdef GDI_WITH_TESTSAVE
  bitmap.Save(L"D:/Data/bitmapTest.png", &pngEncoder, nullptr);
  bitmap.Save(L"D:/Data/bitmapTest.bmp", &bmpEncoder, nullptr);
  bitmap.Save(L"D:/Data/bitmapTest.tif", &tifEncoder, nullptr);
#endif // GDI_WITH_TESTSAVE

  if (bitmap.GetLastStatus() != Gdiplus::Ok) return nullptr;
  const Gdiplus::Color color = Gdiplus::Color::White;
  if (bitmap.GetHBITMAP(color, &result) != Gdiplus::Ok) return nullptr;
  return result;
}