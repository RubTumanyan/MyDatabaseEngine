#pragma once
#include "page.h"
#include <string>
#include <fstream>
#include <stdexcept>

namespace mydb {

// DiskManager handles raw file I/O — reads and writes pages to disk.
// Each database file is a sequence of fixed-size PAGE_SIZE blocks.
class DiskManager {
public:
    explicit DiskManager(const std::string& filename);
    ~DiskManager();

    // Read page from disk into the given Page object
    void readPage(PageId pageId, Page& page);

    // Write the given Page object to disk
    void writePage(PageId pageId, const Page& page);

    // Allocate a new page at the end of the file — returns its id
    PageId allocatePage();

    // Total number of pages in the file
    PageId pageCount() const { return pageCount_; }

    const std::string& filename() const { return filename_; }

private:
    std::string  filename_;
    std::fstream file_;
    PageId       pageCount_ = 0;

    // Byte offset of a page in the file
    std::streamoff pageOffset(PageId id) const {
        return static_cast<std::streamoff>(id) * PAGE_SIZE;
    }
};

} // namespace mydb