/*
 *  Copyright (C) 2026 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "../src/PdfPages.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <jpeglib.h>

namespace
{
void Require(bool condition, const char* message)
{
  if (!condition)
  {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(EXIT_FAILURE);
  }
}

/*!
 * Two pages: a landscape one, 200 by 100 points, whose left half is black,
 * and a portrait one, 100 by 200 points, left blank
 */
std::vector<char> TwoPagePdf()
{
  const std::string content = "0 0 0 rg 0 0 100 100 re f";
  const std::vector<std::string> objects = {
      "<< /Type /Catalog /Pages 2 0 R >>",
      "<< /Type /Pages /Kids [3 0 R 5 0 R] /Count 2 >>",
      "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 100] /Contents 4 0 R >>",
      "<< /Length " + std::to_string(content.size()) + " >>\nstream\n" + content + "\nendstream",
      "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 100 200] >>",
  };
  std::string pdf = "%PDF-1.4\n";
  std::vector<size_t> offsets;
  for (size_t i = 0; i < objects.size(); i++)
  {
    offsets.push_back(pdf.size());
    pdf += std::to_string(i + 1) + " 0 obj\n" + objects[i] + "\nendobj\n";
  }
  const size_t xref = pdf.size();
  pdf += "xref\n0 " + std::to_string(objects.size() + 1) + "\n0000000000 65535 f \n";
  for (const size_t offset : offsets)
  {
    char line[24];
    std::snprintf(line, sizeof(line), "%010zu 00000 n \n", offset);
    pdf += line;
  }
  pdf += "trailer\n<< /Size " + std::to_string(objects.size() + 1) + " /Root 1 0 R >>\n" +
         "startxref\n" + std::to_string(xref) + "\n%%EOF\n";
  return {pdf.begin(), pdf.end()};
}

struct Picture
{
  unsigned int width{0};
  unsigned int height{0};
  std::vector<unsigned char> rgb;

  int Brightness(unsigned int x, unsigned int y) const
  {
    const size_t i = (static_cast<size_t>(y) * width + x) * 3;
    return (rgb[i] + rgb[i + 1] + rgb[i + 2]) / 3;
  }
};

Picture Decode(const std::vector<uint8_t>& jpeg)
{
  jpeg_decompress_struct cinfo{};
  jpeg_error_mgr jerr{};
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);
  jpeg_mem_src(&cinfo, jpeg.data(), static_cast<unsigned long>(jpeg.size()));
  Require(jpeg_read_header(&cinfo, TRUE) == JPEG_HEADER_OK, "not a JPEG");
  cinfo.out_color_space = JCS_RGB;
  jpeg_start_decompress(&cinfo);
  Picture picture;
  picture.width = cinfo.output_width;
  picture.height = cinfo.output_height;
  picture.rgb.resize(static_cast<size_t>(picture.width) * picture.height * 3);
  while (cinfo.output_scanline < cinfo.output_height)
  {
    JSAMPROW row = &picture.rgb[static_cast<size_t>(cinfo.output_scanline) * picture.width * 3];
    jpeg_read_scanlines(&cinfo, &row, 1);
  }
  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  return picture;
}
} // namespace

int main()
{
  std::string error;
  std::unique_ptr<CPdfPages> pages = CPdfPages::Load(TwoPagePdf(), error);
  Require(pages != nullptr, "the PDF did not load");
  Require(pages->PageCount() == 2, "wrong page count");

  std::vector<uint8_t> jpeg;
  Require(pages->RenderJpeg(0, 400, jpeg), "page 1 did not render");
  const Picture landscape = Decode(jpeg);
  Require(landscape.width == 400 && landscape.height == 200,
          "the longer side must fit the size asked for, keeping the shape");
  Require(landscape.Brightness(50, 100) < 40, "the drawn half must be dark");
  Require(landscape.Brightness(350, 100) > 215, "the rest must be white paper");

  Require(pages->RenderJpeg(1, 400, jpeg), "page 2 did not render");
  const Picture portrait = Decode(jpeg);
  Require(portrait.width == 200 && portrait.height == 400, "a portrait page fits by its height");

  Require(!pages->RenderJpeg(2, 400, jpeg), "there is no third page");

  std::vector<char> garbage{'n', 'o', 't', ' ', 'a', ' ', 'p', 'd', 'f'};
  Require(CPdfPages::Load(garbage, error) == nullptr && !error.empty(),
          "something that is not a PDF must be refused with a reason");

  std::puts("PASS: pages render to the size asked for, on white, and junk is refused");
  return EXIT_SUCCESS;
}
