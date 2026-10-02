/*
 *  Copyright (C) 2026 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace poppler
{
class document;
}

/*!
 * \brief The pages of one PDF, drawn as pictures
 *
 * Knows nothing of Kodi, so it can be tested on its own.
 */
class CPdfPages
{
public:
  ~CPdfPages();

  /*!
   * \brief Read a PDF from its bytes
   *
   * \param data The whole file, which the document then reads in place
   * \param error Set to why it could not be read
   *
   * \return The document, or nothing if it is not a PDF that can be shown
   */
  static std::unique_ptr<CPdfPages> Load(std::vector<char> data, std::string& error);

  unsigned int PageCount() const { return m_pageCount; }

  /*!
   * \brief Draw a page onto white paper, as a JPEG
   *
   * \param page The page, counting from 0
   * \param maxSide The longest the picture's longer side may be, in pixels
   * \param jpeg Receives the picture
   */
  bool RenderJpeg(unsigned int page, unsigned int maxSide, std::vector<uint8_t>& jpeg) const;

private:
  CPdfPages(std::vector<char> data, std::unique_ptr<poppler::document> document);

  std::vector<char> m_data;
  std::unique_ptr<poppler::document> m_document;
  unsigned int m_pageCount{0};
  //! Poppler draws one page of a document at a time
  mutable std::mutex m_mutex;
};
