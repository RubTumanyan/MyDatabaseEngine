#include "disk_manager.h"
#include <cstring>

namespace mydb {

DiskManager::DiskManager(const std::string& filename)
    : filename_(filename) {

    // Open existing file or create new one
    file_.open(filename,
               std::ios::in | std::ios::out |
               std::ios::binary);

    if (!file_.is_open()) {
        // File doesn't exist — create it
        file_.open(filename,
                   std::ios::in | std::ios::out |
                   std::ios::binary | std::ios::trunc);
    }

    if (!file_.is_open())
        throw std::runtime_error("Cannot open database file: " + filename);

    // Calculate how many pages already exist in the file
    file_.seekg(0, std::ios::end);
    std::streamoff size = file_.tellg();
    pageCount_ = static_cast<PageId>(size / PAGE_SIZE);
}

DiskManager::~DiskManager() {
    if (file_.is_open())
        file_.close();
}

void DiskManager::readPage(PageId pageId, Page& page) {
    if (pageId >= pageCount_)
        throw std::runtime_error("Page " + std::to_string(pageId) +
                                 " does not exist");

    file_.seekg(pageOffset(pageId));
    file_.read(page.rawData(), PAGE_SIZE);

    if (!file_)
        throw std::runtime_error("Failed to read page " +
                                 std::to_string(pageId));
}

void DiskManager::writePage(PageId pageId, const Page& page) {
    file_.seekp(pageOffset(pageId));
    file_.write(page.rawData(), PAGE_SIZE);
    file_.flush(); // ensure data reaches disk

    if (!file_)
        throw std::runtime_error("Failed to write page " +
                                 std::to_string(pageId));

    // If writing beyond current end — update page count
    if (pageId >= pageCount_)
        pageCount_ = pageId + 1;
}

PageId DiskManager::allocatePage() {
    PageId newId = pageCount_;

    // Write empty page to extend the file
    Page empty(newId);
    writePage(newId, empty);

    return newId;
}

} // namespace mydb
