#include "table_heap.h"
#include <cstring>
#include <sstream>

namespace mydb {

// ── Page layout constants ────────────────────────────────────────
// After the standard PageHeader:
//   [slotCount : uint16_t]
//   [SlotEntry0][SlotEntry1]...  (5 bytes each, grow forward)
//   ... free space ...
//   [row data from end of page, grow backward]
struct SlotEntry {
    uint16_t offset;
    uint16_t length;
    uint8_t  valid;
};

static constexpr size_t SLOT_DIR_START = sizeof(PageHeader) + sizeof(uint16_t);
static constexpr size_t SLOT_SIZE      = sizeof(SlotEntry);

// ── Serialization ────────────────────────────────────────────────
std::string TableHeap::serializeRowData(const Row& row) {
    std::string result;
    for (const auto& [col, val] : row)
        result += col + "=" + val + ";";
    return result;
}

Row TableHeap::deserializeRowData(const std::string& s) {
    Row row;
    size_t start = 0;
    while (start < s.size()) {
        size_t end = s.find(';', start);
        if (end == std::string::npos) break;
        std::string token = s.substr(start, end - start);
        start = end + 1;
        if (token.empty()) continue;
        auto eq = token.find('=');
        if (eq != std::string::npos) {
            std::string col = token.substr(0, eq);
            std::string val = token.substr(eq + 1);
            if (!col.empty()) row[col] = val;
        }
    }
    return row;
}

// ── Constructor / Factory ────────────────────────────────────────
TableHeap::TableHeap(BufferPool& pool, PageId firstPageId)
    : pool_(pool), firstPageId_(firstPageId) {}

TableHeap TableHeap::create(BufferPool& pool) {
    Page* page = pool.newPage();
    PageId id = page->id();
    pool.unpin(id, true);
    return TableHeap(pool, id);
}

// ── Insert ───────────────────────────────────────────────────────
RowId TableHeap::insertRow(const Row& row) {
    Page* page = pool_.fetchPage(firstPageId_);
    RowId rid = insertIntoPage(page, row);
    pool_.unpin(firstPageId_, true);
    return rid;
}

RowId TableHeap::insertIntoPage(Page* page, const Row& row) {
    std::string data = serializeRowData(row);
    uint16_t dataLen = static_cast<uint16_t>(data.size());

    uint16_t slotCount = page->header().recordCount;
    size_t slotDirEnd  = SLOT_DIR_START + slotCount * SLOT_SIZE;

    // Find data start (end of used data region, growing backward)
    uint16_t dataStart = PAGE_SIZE;
    for (uint16_t i = 0; i < slotCount; ++i) {
        SlotEntry e;
        std::memcpy(&e, page->rawData() + SLOT_DIR_START + i * SLOT_SIZE, SLOT_SIZE);
        if (e.valid && e.offset < dataStart)
            dataStart = e.offset;
    }

    // Check free space: need SLOT_SIZE for slot + dataLen for data
    if (slotDirEnd + SLOT_SIZE > dataStart - dataLen) {
        // Page full — return invalid (caller should allocate new page)
        return RowId{INVALID_PAGE_ID, 0};
    }

    // Write row data at the beginning of the data region
    uint16_t rowOffset = dataStart - dataLen;
    std::memcpy(page->rawData() + rowOffset, data.data(), dataLen);

    // Write slot entry
    SlotEntry entry{rowOffset, dataLen, 1};
    std::memcpy(page->rawData() + slotDirEnd, &entry, SLOT_SIZE);

    // Update page header
    page->header().recordCount = slotCount + 1;

    return RowId{page->id(), slotCount};
}

// ── Get ──────────────────────────────────────────────────────────
std::optional<Row> TableHeap::getRow(const RowId& rid) const {
    Page* page = pool_.fetchPage(rid.pageId);
    uint16_t slotCount = page->header().recordCount;
    pool_.unpin(rid.pageId, false);

    if (rid.slotIndex >= slotCount) return std::nullopt;

    SlotEntry entry;
    std::memcpy(&entry,
                page->rawData() + SLOT_DIR_START + rid.slotIndex * SLOT_SIZE,
                SLOT_SIZE);

    if (!entry.valid) return std::nullopt;

    std::string data(page->rawData() + entry.offset, entry.length);
    return deserializeRowData(data);
}

// ── Update ───────────────────────────────────────────────────────
bool TableHeap::updateRow(const RowId& rid, const Row& newRow) {
    Page* page = pool_.fetchPage(rid.pageId);
    uint16_t slotCount = page->header().recordCount;

    if (rid.slotIndex >= slotCount) {
        pool_.unpin(rid.pageId, false);
        return false;
    }

    // Mark old slot as invalid (simple delete + re-insert)
    SlotEntry oldEntry;
    std::memcpy(&oldEntry,
                page->rawData() + SLOT_DIR_START + rid.slotIndex * SLOT_SIZE,
                SLOT_SIZE);

    if (!oldEntry.valid) {
        pool_.unpin(rid.pageId, false);
        return false;
    }

    oldEntry.valid = 0;
    std::memcpy(page->rawData() + SLOT_DIR_START + rid.slotIndex * SLOT_SIZE,
                &oldEntry, SLOT_SIZE);

    pool_.unpin(rid.pageId, true);

    // Insert new row (may land on same or different page)
    insertRow(newRow);
    return true;
}

// ── Delete ───────────────────────────────────────────────────────
bool TableHeap::deleteRow(const RowId& rid) {
    Page* page = pool_.fetchPage(rid.pageId);
    uint16_t slotCount = page->header().recordCount;

    if (rid.slotIndex >= slotCount) {
        pool_.unpin(rid.pageId, false);
        return false;
    }

    SlotEntry entry;
    std::memcpy(&entry,
                page->rawData() + SLOT_DIR_START + rid.slotIndex * SLOT_SIZE,
                SLOT_SIZE);

    if (!entry.valid) {
        pool_.unpin(rid.pageId, false);
        return false;
    }

    entry.valid = 0;
    std::memcpy(page->rawData() + SLOT_DIR_START + rid.slotIndex * SLOT_SIZE,
                &entry, SLOT_SIZE);

    pool_.unpin(rid.pageId, true);
    return true;
}

// ── Scan ─────────────────────────────────────────────────────────
std::vector<std::pair<RowId, Row>> TableHeap::scan(
    const MvccSnapshot* snapshot) const {

    std::vector<std::pair<RowId, Row>> results;
    PageId pid = firstPageId_;

    while (pid != INVALID_PAGE_ID) {
        Page* page = pool_.fetchPage(pid);
        uint16_t slotCount = page->header().recordCount;

        for (uint16_t i = 0; i < slotCount; ++i) {
            SlotEntry entry;
            std::memcpy(&entry,
                        page->rawData() + SLOT_DIR_START + i * SLOT_SIZE,
                        SLOT_SIZE);

            if (!entry.valid) continue;

            std::string data(page->rawData() + entry.offset, entry.length);
            Row row = deserializeRowData(data);

            // MVCC visibility check (wrap row as visible VersionedRow)
            if (snapshot) {
                VersionedRow vr{row, 1, 0};
                if (!snapshot->isVisible(vr)) continue;
            }

            results.emplace_back(RowId{pid, i}, std::move(row));
        }

        PageId nextPid = pid + 1;
        pool_.unpin(pid, false);
        pid = nextPid;
    }

    return results;
}

} // namespace mydb
