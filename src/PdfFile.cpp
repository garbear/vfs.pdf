/*
 *  Copyright (C) 2026 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "PdfPages.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <kodi/Filesystem.h>
#include <kodi/General.h>
#include <kodi/addon-instance/VFS.h>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

/*
 * A PDF opens as a folder with a picture of each page in it:
 *
 *   pdf://<the PDF's path, URL encoded>/0001.jpg
 */
namespace
{
constexpr const char* PROTOCOL = "pdf://";
//! The longer side of a page's picture. Room to zoom in on a manual's small
//! print, and within the 4096 that is still a common texture limit.
constexpr unsigned int PAGE_SIZE = 2560;
//! A manual is a handful of megabytes; anything this size is not one
constexpr int64_t MAX_DOCUMENT_SIZE = 256 * 1024 * 1024;
constexpr size_t DOCUMENTS_KEPT = 3;
//! Kodi opens a picture more than once on its way to the screen, and a viewer
//! turns back a page as often as forward
constexpr size_t PAGES_KEPT = 8;

std::string URLEncode(const std::string& data)
{
  std::string result;
  result.reserve(data.length() * 2);
  for (const unsigned char c : data)
  {
    if (std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '!' || c == '(' || c == ')')
      result.push_back(static_cast<char>(c));
    else
    {
      char escaped[4];
      snprintf(escaped, sizeof(escaped), "%%%02x", c);
      result += escaped;
    }
  }
  return result;
}

std::string PageName(unsigned int page)
{
  char name[16];
  snprintf(name, sizeof(name), "%04u.jpg", page + 1);
  return name;
}

//! The page a picture's name stands for, counting from 0, or -1
int PageIndex(const std::string& filename)
{
  unsigned int number = 0;
  char extension[8] = {};
  if (std::sscanf(filename.c_str(), "%u.%7s", &number, extension) != 2 || number == 0 ||
      std::string(extension) != "jpg" || filename != PageName(number - 1))
    return -1;
  return static_cast<int>(number - 1);
}

/*!
 * \brief The documents and pages read most recently, shared by every instance
 */
class CCache
{
public:
  std::shared_ptr<CPdfPages> Document(const std::string& path)
  {
    kodi::vfs::FileStatus status;
    const bool known = kodi::vfs::StatFile(path, status);
    const std::string version =
        known ? std::to_string(status.GetSize()) + ":" +
                    std::to_string(static_cast<long long>(status.GetModificationTime()))
              : std::string();

    std::unique_lock<std::mutex> lock(m_mutex);
    for (auto it = m_documents.begin(); it != m_documents.end(); ++it)
    {
      if (it->path == path && it->version == version)
      {
        m_documents.splice(m_documents.begin(), m_documents, it);
        return it->pages;
      }
    }
    lock.unlock();

    std::shared_ptr<CPdfPages> pages = Read(path);
    if (!pages)
      return {};

    lock.lock();
    m_documents.remove_if([&path](const CachedDocument& d) { return d.path == path; });
    m_rendered.remove_if([&path](const CachedPage& p) { return p.path == path; });
    m_documents.push_front({path, version, pages});
    if (m_documents.size() > DOCUMENTS_KEPT)
      m_documents.pop_back();
    return pages;
  }

  std::shared_ptr<const std::vector<uint8_t>> Picture(const std::string& path, unsigned int page)
  {
    std::shared_ptr<CPdfPages> pages = Document(path);
    if (!pages || page >= pages->PageCount())
      return {};

    std::unique_lock<std::mutex> lock(m_mutex);
    for (auto it = m_rendered.begin(); it != m_rendered.end(); ++it)
    {
      if (it->path == path && it->page == page)
      {
        m_rendered.splice(m_rendered.begin(), m_rendered, it);
        return it->jpeg;
      }
    }
    lock.unlock();

    auto jpeg = std::make_shared<std::vector<uint8_t>>();
    if (!pages->RenderJpeg(page, PAGE_SIZE, *jpeg))
    {
      kodi::Log(ADDON_LOG_ERROR, "Page %u of \"%s\" did not render", page + 1, path.c_str());
      return {};
    }

    lock.lock();
    m_rendered.push_front({path, page, jpeg});
    if (m_rendered.size() > PAGES_KEPT)
      m_rendered.pop_back();
    return jpeg;
  }

private:
  struct CachedDocument
  {
    std::string path;
    std::string version;
    std::shared_ptr<CPdfPages> pages;
  };

  struct CachedPage
  {
    std::string path;
    unsigned int page;
    std::shared_ptr<const std::vector<uint8_t>> jpeg;
  };

  static std::shared_ptr<CPdfPages> Read(const std::string& path)
  {
    kodi::vfs::CFile file;
    if (!file.OpenFile(path))
    {
      kodi::Log(ADDON_LOG_ERROR, "Failed to open \"%s\"", path.c_str());
      return {};
    }
    const int64_t length = file.GetLength();
    if (length <= 0 || length > MAX_DOCUMENT_SIZE)
    {
      kodi::Log(ADDON_LOG_ERROR, "Refusing \"%s\", size %lld bytes", path.c_str(),
                static_cast<long long>(length));
      return {};
    }
    std::vector<char> data(static_cast<size_t>(length));
    size_t total = 0;
    while (total < data.size())
    {
      const ssize_t read = file.Read(data.data() + total, data.size() - total);
      if (read <= 0)
        break;
      total += static_cast<size_t>(read);
    }
    if (total != data.size())
    {
      kodi::Log(ADDON_LOG_ERROR, "Read %zu of %zu bytes from \"%s\"", total, data.size(),
                path.c_str());
      return {};
    }

    std::string error;
    std::unique_ptr<CPdfPages> pages = CPdfPages::Load(std::move(data), error);
    if (!pages)
      kodi::Log(ADDON_LOG_ERROR, "\"%s\": %s", path.c_str(), error.c_str());
    return pages;
  }

  std::mutex m_mutex;
  std::list<CachedDocument> m_documents;
  std::list<CachedPage> m_rendered;
};

CCache& Cache()
{
  static CCache cache;
  return cache;
}

struct OpenPage
{
  std::shared_ptr<const std::vector<uint8_t>> jpeg;
  int64_t position{0};
};

void ListPages(const std::string& path,
               const std::string& root,
               std::vector<kodi::vfs::CDirEntry>& items)
{
  std::shared_ptr<CPdfPages> pages = Cache().Document(path);
  if (!pages)
    return;
  for (unsigned int page = 0; page < pages->PageCount(); page++)
  {
    const std::string name = PageName(page);
    items.emplace_back(name, root + name, false);
  }
}
} // namespace

class ATTR_DLL_LOCAL CPdfFile : public kodi::addon::CInstanceVFS
{
public:
  CPdfFile(const kodi::addon::IInstanceInfo& instance) : CInstanceVFS(instance) {}

  kodi::addon::VFSFileHandle Open(const kodi::addon::VFSUrl& url) override
  {
    const int page = PageIndex(url.GetFilename());
    if (page < 0)
      return nullptr;
    auto jpeg = Cache().Picture(url.GetHostname(), static_cast<unsigned int>(page));
    if (!jpeg)
      return nullptr;
    return new OpenPage{std::move(jpeg)};
  }

  ssize_t Read(kodi::addon::VFSFileHandle context, uint8_t* buffer, size_t size) override
  {
    auto* open = static_cast<OpenPage*>(context);
    if (!open)
      return -1;
    const int64_t left = static_cast<int64_t>(open->jpeg->size()) - open->position;
    const size_t count = static_cast<size_t>(std::max<int64_t>(0, std::min<int64_t>(left, size)));
    std::copy_n(open->jpeg->data() + open->position, count, buffer);
    open->position += static_cast<int64_t>(count);
    return static_cast<ssize_t>(count);
  }

  int64_t Seek(kodi::addon::VFSFileHandle context, int64_t position, int whence) override
  {
    auto* open = static_cast<OpenPage*>(context);
    if (!open)
      return -1;
    const int64_t length = static_cast<int64_t>(open->jpeg->size());
    int64_t target = position;
    if (whence == SEEK_CUR)
      target += open->position;
    else if (whence == SEEK_END)
      target += length;
    if (target < 0 || target > length)
      return -1;
    open->position = target;
    return target;
  }

  int64_t GetLength(kodi::addon::VFSFileHandle context) override
  {
    auto* open = static_cast<OpenPage*>(context);
    return open ? static_cast<int64_t>(open->jpeg->size()) : -1;
  }

  int64_t GetPosition(kodi::addon::VFSFileHandle context) override
  {
    auto* open = static_cast<OpenPage*>(context);
    return open ? open->position : -1;
  }

  bool Close(kodi::addon::VFSFileHandle context) override
  {
    delete static_cast<OpenPage*>(context);
    return true;
  }

  int Stat(const kodi::addon::VFSUrl& url, kodi::vfs::FileStatus& buffer) override { return -1; }

  bool Exists(const kodi::addon::VFSUrl& url) override
  {
    const int page = PageIndex(url.GetFilename());
    if (page < 0)
      return false;
    std::shared_ptr<CPdfPages> pages = Cache().Document(url.GetHostname());
    return pages && static_cast<unsigned int>(page) < pages->PageCount();
  }

  bool DirectoryExists(const kodi::addon::VFSUrl& url) override
  {
    return url.GetFilename().empty() && Cache().Document(url.GetHostname()) != nullptr;
  }

  bool GetDirectory(const kodi::addon::VFSUrl& url,
                    std::vector<kodi::vfs::CDirEntry>& items,
                    CVFSCallbacks callbacks) override
  {
    if (!url.GetFilename().empty())
      return false;
    std::string root = url.GetURL();
    if (root.empty() || root.back() != '/')
      root += '/';
    ListPages(url.GetHostname(), root, items);
    return !items.empty();
  }

  bool ContainsFiles(const kodi::addon::VFSUrl& url,
                     std::vector<kodi::vfs::CDirEntry>& items,
                     std::string& rootpath) override
  {
    rootpath = PROTOCOL + URLEncode(url.GetURL()) + "/";
    ListPages(url.GetURL(), rootpath, items);
    return !items.empty();
  }
};

class ATTR_DLL_LOCAL CMyAddon : public kodi::addon::CAddonBase
{
public:
  CMyAddon() = default;
  ADDON_STATUS CreateInstance(const kodi::addon::IInstanceInfo& instance,
                              KODI_ADDON_INSTANCE_HDL& hdl) override
  {
    hdl = new CPdfFile(instance);
    return ADDON_STATUS_OK;
  }
};

ADDONCREATOR(CMyAddon)
