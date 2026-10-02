/*
 *  Copyright (C) 2026 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "PdfPages.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include <jpeglib.h>
#include <poppler-document.h>
#include <poppler-image.h>
#include <poppler-page-renderer.h>
#include <poppler-page.h>

namespace
{
//! PDF sizes are in points, 72 to the inch
constexpr double POINTS_PER_INCH = 72.0;
constexpr int JPEG_QUALITY = 90;

bool EncodeJpeg(const poppler::image& image, std::vector<uint8_t>& jpeg)
{
  jpeg_compress_struct cinfo{};
  jpeg_error_mgr jerr{};
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);

  unsigned char* buffer = nullptr;
  unsigned long size = 0;
  jpeg_mem_dest(&cinfo, &buffer, &size);

  cinfo.image_width = static_cast<JDIMENSION>(image.width());
  cinfo.image_height = static_cast<JDIMENSION>(image.height());
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;
  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, JPEG_QUALITY, TRUE);
  jpeg_start_compress(&cinfo, TRUE);

  // Poppler's argb32 is a native 0xAARRGGBB word per pixel; the page is drawn
  // opaque on white, so alpha is dropped
  std::vector<JSAMPLE> row(static_cast<size_t>(image.width()) * 3);
  const char* pixels = image.const_data();
  for (int y = 0; y < image.height(); y++)
  {
    const auto* line = reinterpret_cast<const uint32_t*>(pixels + y * image.bytes_per_row());
    for (int x = 0; x < image.width(); x++)
    {
      row[x * 3] = static_cast<JSAMPLE>((line[x] >> 16) & 0xff);
      row[x * 3 + 1] = static_cast<JSAMPLE>((line[x] >> 8) & 0xff);
      row[x * 3 + 2] = static_cast<JSAMPLE>(line[x] & 0xff);
    }
    JSAMPROW rows[] = {row.data()};
    jpeg_write_scanlines(&cinfo, rows, 1);
  }

  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);

  jpeg.assign(buffer, buffer + size);
  free(buffer);
  return !jpeg.empty();
}
} // namespace

CPdfPages::CPdfPages(std::vector<char> data, std::unique_ptr<poppler::document> document)
  : m_data(std::move(data)),
    m_document(std::move(document))
{
  const int pages = m_document->pages();
  m_pageCount = pages > 0 ? static_cast<unsigned int>(pages) : 0;
}

CPdfPages::~CPdfPages() = default;

std::unique_ptr<CPdfPages> CPdfPages::Load(std::vector<char> data, std::string& error)
{
  std::unique_ptr<poppler::document> document(
      poppler::document::load_from_raw_data(data.data(), static_cast<int>(data.size())));
  if (!document)
  {
    error = "not a readable PDF";
    return {};
  }
  // There is nowhere to ask for a password
  if (document->is_locked())
  {
    error = "password protected";
    return {};
  }
  if (document->pages() <= 0)
  {
    error = "no pages";
    return {};
  }
  return std::unique_ptr<CPdfPages>(new CPdfPages(std::move(data), std::move(document)));
}

bool CPdfPages::RenderJpeg(unsigned int page, unsigned int maxSide, std::vector<uint8_t>& jpeg) const
{
  if (page >= m_pageCount || maxSide == 0)
    return false;

  std::unique_lock<std::mutex> lock(m_mutex);

  std::unique_ptr<poppler::page> pdfPage(m_document->create_page(static_cast<int>(page)));
  if (!pdfPage)
    return false;

  const poppler::rectf rect = pdfPage->page_rect();
  if (rect.width() <= 0.0 || rect.height() <= 0.0)
    return false;

  const double scale = maxSide / std::max(rect.width(), rect.height());
  const double dpi = scale * POINTS_PER_INCH;

  poppler::page_renderer renderer;
  renderer.set_render_hint(poppler::page_renderer::antialiasing, true);
  renderer.set_render_hint(poppler::page_renderer::text_antialiasing, true);
  // A scan without a background would otherwise be dark text on transparency
  renderer.set_paper_color(0xffffffff);

  const poppler::image image = renderer.render_page(pdfPage.get(), dpi, dpi);
  lock.unlock();

  if (!image.is_valid() || image.format() != poppler::image::format_argb32)
    return false;

  return EncodeJpeg(image, jpeg);
}
