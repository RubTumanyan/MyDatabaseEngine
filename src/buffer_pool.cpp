#include "buffer_pool.h"

namespace mydb {

BufferPool::BufferPool(size_t poolSize, DiskManager& disk)
    : poolSize_(poolSize), disk_(disk) {}

Page* BufferPool::fetchPage(PageId pageId) {
    // Cache hit — page already in memory
    auto it = pages_.find(pageId);
    if (it != pages_.end()) {
        touchPage(pageId);        // mark as recently used
        it->second->pin();
        return it->second.get();
    }

    // Cache miss — need to load from disk
    // Evict if pool is full
    if (pages_.size() >= poolSize_) {
        if (!evict())
            throw std::runtime_error("Buffer pool full — all pages pinned");
    }

    // Load page from disk
    auto page = std::make_unique<Page>(pageId);
    disk_.readPage(pageId, *page);
    page->pin();

    Page* raw = page.get();
    pages_[pageId] = std::move(page);

    // Add to front of LRU list
    lruList_.push_front(pageId);
    lruMap_[pageId] = lruList_.begin();

    return raw;
}

Page* BufferPool::newPage() {
    // Evict if pool is full
    if (pages_.size() >= poolSize_) {
        if (!evict())
            throw std::runtime_error("Buffer pool full — all pages pinned");
    }

    // Allocate new page on disk
    PageId newId = disk_.allocatePage();

    auto page = std::make_unique<Page>(newId);
    page->pin();
    page->setDirty(true); // new page must be written to disk

    Page* raw = page.get();
    pages_[newId] = std::move(page);

    lruList_.push_front(newId);
    lruMap_[newId] = lruList_.begin();

    return raw;
}

void BufferPool::unpin(PageId pageId, bool isDirty) {
    auto it = pages_.find(pageId);
    if (it == pages_.end()) return;

    it->second->unpin();
    if (isDirty) it->second->setDirty(true);
}

void BufferPool::flushPage(PageId pageId) {
    auto it = pages_.find(pageId);
    if (it == pages_.end()) return;

    if (it->second->isDirty()) {
        disk_.writePage(pageId, *it->second);
        it->second->setDirty(false);
    }
}

void BufferPool::flushAll() {
    for (auto& [pageId, page] : pages_) {
        if (page->isDirty()) {
            disk_.writePage(pageId, *page);
            page->setDirty(false);
        }
    }
}

void BufferPool::touchPage(PageId pageId) {
    // Remove from current position and move to front
    auto it = lruMap_.find(pageId);
    if (it != lruMap_.end()) {
        lruList_.erase(it->second);
    }
    lruList_.push_front(pageId);
    lruMap_[pageId] = lruList_.begin();
}

bool BufferPool::evict() {
    // Walk from back (least recently used) looking for unpinned page
    for (auto it = lruList_.rbegin(); it != lruList_.rend(); ++it) {
        PageId    candidate = *it;
        auto&     page      = pages_[candidate];

        if (page->pinCount() > 0) continue; // skip pinned pages

        // Write to disk if dirty before evicting
        if (page->isDirty())
            disk_.writePage(candidate, *page);

        // Remove from all data structures
        pages_.erase(candidate);
        lruMap_.erase(candidate);
        lruList_.erase(std::next(it).base());
        return true;
    }
    return false; // all pages pinned
}

} // namespace mydb