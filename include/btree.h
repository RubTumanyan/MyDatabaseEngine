#pragma once
#include "page.h"
#include "buffer_pool.h"
#include <string>
#include <vector>
#include <optional>

namespace mydb {

// B-Tree node — stored on a single page
// Internal nodes: keys + child page ids
// Leaf nodes:     keys + record ids (pointers to actual rows)
struct BTreeNode {
    static constexpr int ORDER = 4; // max keys per node

    bool     isLeaf   = true;
    int      keyCount = 0;
    std::string keys[ORDER + 1];         // keys
    PageId   children[ORDER + 2]{};      // child page ids (internal only)
    std::string values[ORDER + 1];       // values at leaf (row id or data)
    PageId   nextLeaf = INVALID_PAGE_ID; // linked list of leaf nodes

    // Serialize to/from raw page bytes
    void serialize(Page& page) const;
    void deserialize(const Page& page);
};

// B-Tree index — maps string keys to string values (row ids)
// Supports: insert, search, range scan
class BTree {
public:
    explicit BTree(BufferPool& pool);

    // Insert key → value into the index
    void insert(const std::string& key, const std::string& value);

    // Search for exact key — returns value or nullopt
    std::optional<std::string> search(const std::string& key);

    // Range scan — returns all (key, value) pairs where key >= from
    std::vector<std::pair<std::string, std::string>>
    rangeScan(const std::string& from, const std::string& to);

private:
    BufferPool& pool_;
    PageId      rootId_ = INVALID_PAGE_ID;

    // Load/save a node from/to buffer pool
    BTreeNode loadNode(PageId id);
    void      saveNode(PageId id, const BTreeNode& node);

    // Allocate a new page for a node
    PageId allocNode();

    // Recursive insert — returns split key and new page if node split
    struct SplitResult { std::string key; PageId newPage; };
    std::optional<SplitResult> insertRec(PageId nodeId,
                                          const std::string& key,
                                          const std::string& value);

    // Split a full node into two
    SplitResult splitNode(PageId nodeId, BTreeNode& node);

    // Find the leaf page where key should be
    PageId findLeaf(const std::string& key);
};

} // namespace mydb