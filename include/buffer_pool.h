#pragma once
#include "page.h"
#include "disk_manager.h"
#include <unordered_map>
#include <list>
#include <memory>
#include <stdexcept>

namespace mydb {

// BufferPool keeps frequently used pages in memory.
// When full, it evicts the Least Recently Used (LRU) unpinned page.
//
// LRU works like this:
//   every time a page is accessed → move it to the front of the list
//   when we need to evict → take from the back of the list (least recently used)
class BufferPool {
public:
    // poolSize: max pages kept in memory simultaneously
    BufferPool(size_t poolSize, DiskManager& disk);

    // Fetch a page — loads from disk if not already in pool
    // Caller must call unpin() when done
    Page* fetchPage(PageId pageId);

    // Allocate a brand new page on disk and load it into the pool
    Page* newPage();

    // Unpin a page — allows it to be evicted when pool is full
    void unpin(PageId pageId, bool isDirty);

    // Force write a dirty page to disk immediately
    void flushPage(PageId pageId);

    // Write all dirty pages to disk
    void flushAll();

    size_t poolSize()  const { return poolSize_; }
    size_t pagesInPool() const { return pages_.size(); }

private:
    size_t        poolSize_;
    DiskManager&  disk_;

    // pageId → Page object in memory
    std::unordered_map<PageId, std::unique_ptr<Page>> pages_;

    // LRU list — front = most recently used, back = least recently used
    std::list<PageId> lruList_;

    // pageId → iterator into lruList_ for O(1) removal
    std::unordered_map<PageId, std::list<PageId>::iterator> lruMap_;

    // Move a page to the front of the LRU list (mark as recently used)
    void touchPage(PageId pageId);

    // Evict the least recently used unpinned page to make room
    // Returns false if all pages are pinned and nothing can be evicted
    bool evict();
};

} // namespace mydb