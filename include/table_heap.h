#pragma once
#include "page.h"
#include "buffer_pool.h"
#include "row.h"
#include "mvcc.h"
#include <string>
#include <vector>
#include <optional>
#include <utility>

namespace mydb {

struct RowId {
    PageId  pageId    = INVALID_PAGE_ID;
    uint16_t slotIndex = 0;

    bool valid() const { return pageId != INVALID_PAGE_ID; }
};

class TableHeap {
public:
    TableHeap(BufferPool& pool, PageId firstPageId);

    static TableHeap create(BufferPool& pool);

    RowId insertRow(const Row& row);
    std::optional<Row> getRow(const RowId& rid) const;
    bool updateRow(const RowId& rid, const Row& newRow);
    bool deleteRow(const RowId& rid);

    std::vector<std::pair<RowId, Row>> scan(
        const MvccSnapshot* snapshot = nullptr) const;

private:
    BufferPool& pool_;
    PageId firstPageId_;

    RowId insertIntoPage(Page* page, const Row& row);
    static std::string serializeRowData(const Row& row);
    static Row deserializeRowData(const std::string& s);
};

} // namespace mydb
