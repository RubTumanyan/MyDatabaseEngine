#pragma once
#include <cstdint>
#include <cstring>
#include <array>

namespace mydb {

// Page is the fundamental unit of storage — 4KB block
// Everything on disk is read/written in page-sized chunks
static constexpr size_t PAGE_SIZE = 4096; // 4 KB

using PageId = uint32_t;
static constexpr PageId INVALID_PAGE_ID = UINT32_MAX;

// Page header — metadata stored at the start of every page
struct PageHeader {
    PageId   pageId;        // which page this is
    uint16_t freeSpace;     // bytes available for new records
    uint16_t recordCount;   // number of records on this page
    uint32_t checksum;      // data integrity check
};

// A page is a fixed-size block of raw bytes
// The first sizeof(PageHeader) bytes are the header
// The rest is data (rows stored as serialized bytes)
class Page {
public:
    explicit Page(PageId id) {
        std::memset(data_.data(), 0, PAGE_SIZE);
        header().pageId      = id;
        header().freeSpace   = PAGE_SIZE - sizeof(PageHeader);
        header().recordCount = 0;
        header().checksum    = 0;
    }

    Page() {
        std::memset(data_.data(), 0, PAGE_SIZE);
    }

    PageId   id()          const { return header().pageId;      }
    uint16_t freeSpace()   const { return header().freeSpace;   }
    uint16_t recordCount() const { return header().recordCount; }

    // Raw access to the page data — used by DiskManager for I/O
    char*       rawData()       { return data_.data(); }
    const char* rawData() const { return data_.data(); }

    // Access header fields directly
    PageHeader&       header()       {
        return *reinterpret_cast<PageHeader*>(data_.data());
    }
    const PageHeader& header() const {
        return *reinterpret_cast<const PageHeader*>(data_.data());
    }

    // Mark page as dirty — needs to be written back to disk
    bool isDirty() const { return dirty_; }
    void setDirty(bool d) { dirty_ = d; }

    // Pin count — how many operators are currently using this page
    // A pinned page cannot be evicted from the buffer pool
    int  pinCount() const { return pinCount_; }
    void pin()            { ++pinCount_; }
    void unpin()          { if (pinCount_ > 0) --pinCount_; }

private:
    std::array<char, PAGE_SIZE> data_;
    bool dirty_    = false;
    int  pinCount_ = 0;
};

} // namespace mydb